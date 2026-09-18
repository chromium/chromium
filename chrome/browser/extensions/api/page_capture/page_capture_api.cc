// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <limits>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/mhtml_generation_params.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::WebContents;
using extensions::PageCaptureSaveAsMHTMLFunction;
using storage::ShareableFileReference;

namespace SaveAsMHTML = extensions::api::page_capture::SaveAsMHTML;

namespace {

const char kFileTooBigError[] = "The MHTML file generated is too big.";
const char kMHTMLGenerationFailedError[] = "Failed to generate MHTML.";
const char kTemporaryFileError[] = "Failed to create a temporary file.";
const char kTabClosedError[] = "Cannot find the tab for this request.";
const char kTabNavigatedError[] =
    "Tab navigated before capture could complete.";
const char kPageCaptureNotAllowed[] =
    "Don't have permissions required to capture this page.";
constexpr base::TaskTraits kCreateTemporaryFileTaskTraits = {
    // Requires IO.
    base::MayBlock(),

    // TaskPriority: Inherit.

    // TaskShutdownBehavior: TemporaryFileCreated*() called from
    // CreateTemporaryFile() might access global variable, so use
    // SKIP_ON_SHUTDOWN. See ShareableFileReference::GetOrCreate().
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

void ClearFileReferenceOnIOThread(
    scoped_refptr<storage::ShareableFileReference>) {}

}  // namespace

static PageCaptureSaveAsMHTMLFunction::TestDelegate* test_delegate_ = nullptr;

PageCaptureSaveAsMHTMLFunction::PageCaptureSaveAsMHTMLFunction() = default;

PageCaptureSaveAsMHTMLFunction::~PageCaptureSaveAsMHTMLFunction() {
  if (mhtml_file_.get()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ClearFileReferenceOnIOThread, std::move(mhtml_file_)));
  }
}

void PageCaptureSaveAsMHTMLFunction::SetTestDelegate(TestDelegate* delegate) {
  test_delegate_ = delegate;
}

ExtensionFunction::ResponseAction PageCaptureSaveAsMHTMLFunction::Run() {
  params_ = SaveAsMHTML::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return RespondNow(Error(kTabClosedError));
  }

  std::string error;
  if (!CanCaptureCurrentPage(*web_contents, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  // Store the document ID for the WebContents to check it hasn't changed by the
  // time we do the capture.
  document_id_ = ExtensionApiFrameIdMap::GetDocumentId(
      web_contents->GetPrimaryMainFrame());

  base::ThreadPool::PostTask(
      FROM_HERE, kCreateTemporaryFileTaskTraits,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile,
                     this));  // Callback increments refcount.
  return RespondLater();
}

bool PageCaptureSaveAsMHTMLFunction::CanCaptureFrame(
    content::RenderFrameHost* render_frame_host) const {
  CHECK(render_frame_host);
  CHECK(extension());
  auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host);
  CHECK(web_contents);

  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  GURL check_url;
  const GURL& committed_url = render_frame_host->GetLastCommittedURL();
  if (render_frame_host->IsInPrimaryMainFrame() &&
      committed_url.SchemeIs(url::kDataScheme)) {
    // Top-level data: URLs must be checked as data: URLs so
    // CanCaptureVisiblePage enforces activeTab even if navigated from an
    // http/https initiator.
    check_url = committed_url;
  } else if (origin.opaque()) {
    const url::SchemeHostPort& precursor =
        origin.GetTupleOrPrecursorTupleIfOpaque();
    if (precursor.IsValid()) {
      check_url = precursor.GetURL();
    } else {
      check_url = committed_url;
    }
  } else {
    check_url = origin.GetURL();
  }

  // Special case: file:// URLs. `CanCaptureVisiblePage()` checks host
  // permissions for file:// URLs, which pageCapture does not require. Instead,
  // check enterprise policy restrictions and whether the extension has file
  // access enabled.
  if (check_url.SchemeIs(url::kFileScheme)) {
    if (extension()->location() != mojom::ManifestLocation::kComponent &&
        extension()->permissions_data()->IsPolicyBlockedHost(check_url)) {
      return false;
    }
    return extensions::util::AllowFileAccess(extension()->id(),
                                             web_contents->GetBrowserContext());
  }

  std::string unused_error;
  return extension()->permissions_data()->CanCaptureVisiblePage(
      check_url, sessions::SessionTabHelper::IdForTab(web_contents).id(),
      &unused_error, extensions::CaptureRequirement::kPageCapture);
}

bool PageCaptureSaveAsMHTMLFunction::CanCaptureCurrentPage(
    WebContents& web_contents,
    std::string* error) const {
  bool can_capture_page = CanCaptureFrame(web_contents.GetPrimaryMainFrame());
  if (!can_capture_page && error) {
    *error = kPageCaptureNotAllowed;
  }
  return can_capture_page;
}

void PageCaptureSaveAsMHTMLFunction::OnResponseAck() {
  // The extension process has processed the response and has created a
  // reference to the blob, it is safe for us to go away.
  // This instance may be deleted after this call, so no code goes after
  // this!!!
  Release();  // Balanced in Run()
}

void PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile() {
  bool success = base::CreateTemporaryFile(&mhtml_path_);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnIO,
                     this, success));
}

void PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnIO(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (success) {
    // Setup a ShareableFileReference so the temporary file gets deleted
    // once it is no longer used.
    mhtml_file_ = ShareableFileReference::GetOrCreate(
        mhtml_path_, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        base::ThreadPool::CreateSequencedTaskRunner(
            {// Requires IO.
             base::MayBlock(),

             // TaskPriority: Inherit.

             // Because we are using DELETE_ON_FINAL_RELEASE here, the
             // storage::ScopedFile inside ShareableFileReference requires
             // a shutdown blocking task runner to ensure that its deletion
             // task runs.
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
            .get());
  }

  // Let the delegate know the reference has been created.
  if (test_delegate_) {
    test_delegate_->OnTemporaryFileCreated(mhtml_file_);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnUI,
                     this, success));
}

void PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnUI(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    ReturnFailure(kTemporaryFileError);
    return;
  }

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    ReturnFailure(kTabClosedError);
    return;
  }
  if (document_id_ != ExtensionApiFrameIdMap::GetDocumentId(
                          web_contents->GetPrimaryMainFrame())) {
    ReturnFailure(kTabNavigatedError);
    return;
  }

  std::string error;
  if (!CanCaptureCurrentPage(*web_contents, &error)) {
    ReturnFailure(error);
    return;
  }

  content::MHTMLGenerationParams params(mhtml_path_);
  params.frame_filter = base::BindRepeating(
      &PageCaptureSaveAsMHTMLFunction::CanCaptureFrame, this);
  web_contents->GenerateMHTML(
      params,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::MHTMLGenerated, this));
}

void PageCaptureSaveAsMHTMLFunction::MHTMLGenerated(int64_t mhtml_file_size) {
  if (mhtml_file_size <= 0) {
    ReturnFailure(kMHTMLGenerationFailedError);
    return;
  }

  if (mhtml_file_size > std::numeric_limits<int>::max()) {
    ReturnFailure(kFileTooBigError);
    return;
  }

  ReturnSuccess(mhtml_file_size);
}

void PageCaptureSaveAsMHTMLFunction::ReturnFailure(const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Respond(Error(error));
}

void PageCaptureSaveAsMHTMLFunction::ReturnSuccess(int file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    ReturnFailure(kTabClosedError);
    return;
  }

  // TODO(crbug.com/379869738) Remove FromUnsafeValue.
  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      content::ChildProcessId::FromUnsafeValue(source_process_id()),
      mhtml_path_);

  base::DictValue response;
  response.Set("mhtmlFilePath", mhtml_path_.AsUTF8Unsafe());
  response.Set("mhtmlFileLength", file_size);
  response.Set("requestId", request_uuid().AsLowercaseString());

  // Add a reference, extending the lifespan of this extension function until
  // the response has been received by the renderer. This function generates a
  // blob which contains a reference scoped to this object. In order for the
  // blob to remain alive, we have to stick around until a reference has
  // been obtained by the renderer. The response ack is the signal that the
  // renderer has it's reference, so we can release ours.
  // TODO(crbug.com/40673405): Potential memory leak here.
  AddRef();  // Balanced in either OnMessageReceived()
  AddResponseTarget();

  Respond(WithArguments(std::move(response)));
}

WebContents* PageCaptureSaveAsMHTMLFunction::GetWebContents() const {
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params_->details.tab_id, browser_context(),
                                    include_incognito_information(),
                                    &web_contents)) {
    return nullptr;
  }
  return web_contents;
}
