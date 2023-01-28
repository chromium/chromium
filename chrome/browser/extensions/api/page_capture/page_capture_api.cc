// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"

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

PageCaptureSaveAsMHTMLFunction::PageCaptureSaveAsMHTMLFunction() {
}

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
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  std::string error;
  if (!CanCaptureCurrentPage(&error)) {
    return RespondNow(Error(std::move(error)));
  }

  base::ThreadPool::PostTask(
      FROM_HERE, kCreateTemporaryFileTaskTraits,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile,
                     this));  // Callback increments refcount.
  return RespondLater();
}

bool PageCaptureSaveAsMHTMLFunction::CanCaptureCurrentPage(std::string* error) {
  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    *error = kTabClosedError;
    return false;
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  const GURL origin_url = url::Origin::Create(url).GetURL();
  bool can_capture_page = false;
  if (origin_url.SchemeIs(url::kFileScheme)) {
    // We special case file schemes, since we don't check for URL permissions
    // in CanCaptureVisiblePage() with the pageCapture API. This ensures
    // file:// URLs are only capturable with the proper permission.
    can_capture_page = extensions::util::AllowFileAccess(
        extension()->id(), web_contents->GetBrowserContext());
  } else {
    std::string unused_error;
    can_capture_page = extension()->permissions_data()->CanCaptureVisiblePage(
        url, sessions::SessionTabHelper::IdForTab(web_contents).id(),
        &unused_error, extensions::CaptureRequirement::kPageCapture);
  }

  if (!can_capture_page) {
    *error = kPageCaptureNotAllowed;
  }
  return can_capture_page;
}

bool PageCaptureSaveAsMHTMLFunction::OnMessageReceived(
    const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ResponseAck::ID)
    return false;

  int message_request_id;
  base::PickleIterator iter(message);
  if (!iter.ReadInt(&message_request_id)) {
    NOTREACHED() << "malformed extension message";
    return true;
  }

  if (message_request_id != request_id())
    return false;

  // The extension process has processed the response and has created a
  // reference to the blob, it is safe for us to go away.
  Release();  // Balanced in Run()

  return true;
}

void PageCaptureSaveAsMHTMLFunction::OnServiceWorkerAck() {
  DCHECK(is_from_service_worker());
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
  if (test_delegate_)
    test_delegate_->OnTemporaryFileCreated(mhtml_file_);

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

  web_contents->GenerateMHTML(
      content::MHTMLGenerationParams(mhtml_path_),
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

void PageCaptureSaveAsMHTMLFunction::ReturnSuccess(int64_t file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    ReturnFailure(kTabClosedError);
    return;
  }

  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(source_process_id(),
                                                           mhtml_path_);

  base::Value response(base::Value::Type::DICT);
  response.SetStringKey("mhtmlFilePath", mhtml_path_.AsUTF8Unsafe());
  response.SetIntKey("mhtmlFileLength", file_size);
  response.SetIntKey("requestId", request_id());

  // Add a reference, extending the lifespan of this extension function until
  // the response has been received by the renderer. This function generates a
  // blob which contains a reference scoped to this object. In order for the
  // blob to remain alive, we have to stick around until a reference has
  // been obtained by the renderer. The response ack is the signal that the
  // renderer has it's reference, so we can release ours.
  // TODO(crbug.com/1050887): Potential memory leak here.
  AddRef();  // Balanced in either OnMessageReceived()
  if (is_from_service_worker())
    AddWorkerResponseTarget();

  Respond(OneArgument(std::move(response)));
}

WebContents* PageCaptureSaveAsMHTMLFunction::GetWebContents() {
  Browser* browser = nullptr;
  content::WebContents* web_contents = nullptr;

  if (!ExtensionTabUtil::GetTabById(params_->details.tab_id, browser_context(),
                                    include_incognito_information(), &browser,
                                    nullptr, &web_contents, nullptr)) {
    return nullptr;
  }
  return web_contents;
}
