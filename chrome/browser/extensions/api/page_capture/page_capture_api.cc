// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"
#endif

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
#if defined(OS_CHROMEOS)
const char kUserDenied[] = "User denied request.";
#endif
constexpr base::TaskTraits kCreateTemporaryFileTaskTraits = {
    base::ThreadPool(),
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

static PageCaptureSaveAsMHTMLFunction::TestDelegate* test_delegate_ = NULL;

PageCaptureSaveAsMHTMLFunction::PageCaptureSaveAsMHTMLFunction() {
}

PageCaptureSaveAsMHTMLFunction::~PageCaptureSaveAsMHTMLFunction() {
  if (mhtml_file_.get()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ClearFileReferenceOnIOThread, std::move(mhtml_file_)));
  }
}

void PageCaptureSaveAsMHTMLFunction::SetTestDelegate(TestDelegate* delegate) {
  test_delegate_ = delegate;
}

bool PageCaptureSaveAsMHTMLFunction::RunAsync() {
  params_ = SaveAsMHTML::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  AddRef();  // Balanced in ReturnFailure/ReturnSuccess()

  // In Public Sessions, extensions (and apps) are force-installed by admin
  // policy so the user does not get a chance to review the permissions for
  // these extensions. This is not acceptable from a security/privacy
  // standpoint, so when an extension uses the PageCapture API for the first
  // time, we show the user a dialog where they can choose whether to allow the
  // extension access to the API.
#if defined(OS_CHROMEOS)
  if (profiles::ArePublicSessionRestrictionsEnabled()) {
    WebContents* web_contents = GetWebContents();
    if (!web_contents) {
      ReturnFailure(kTabClosedError);
      return true;
    }
    // This Unretained is safe because this object is Released() in
    // OnMessageReceived which gets called at some point after callback is run.
    auto callback =
        base::Bind(&PageCaptureSaveAsMHTMLFunction::ResolvePermissionRequest,
                   base::Unretained(this));
    permission_helper::HandlePermissionRequest(
        *extension(), {APIPermission::kPageCapture}, web_contents, callback,
        permission_helper::PromptFactory());
    return true;
  }
#endif

  if (!CanCaptureCurrentPage()) {
    return false;
  }
  base::PostTask(
      FROM_HERE, kCreateTemporaryFileTaskTraits,
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile,
                     this));
  return true;
}

bool PageCaptureSaveAsMHTMLFunction::CanCaptureCurrentPage() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    error_ = kTabClosedError;
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
    std::string error;
    can_capture_page = extension()->permissions_data()->CanCaptureVisiblePage(
        url, SessionTabHelper::IdForTab(web_contents).id(), &error,
        extensions::CaptureRequirement::kPageCapture);
  }

  if (!can_capture_page) {
    error_ = kPageCaptureNotAllowed;
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

#if defined(OS_CHROMEOS)
void PageCaptureSaveAsMHTMLFunction::ResolvePermissionRequest(
    const PermissionIDSet& allowed_permissions) {
  if (allowed_permissions.ContainsID(APIPermission::kPageCapture)) {
    base::PostTask(
        FROM_HERE, kCreateTemporaryFileTaskTraits,
        base::BindOnce(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile,
                       this));
  } else {
    ReturnFailure(kUserDenied);
  }
}
#endif

void PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile() {
  bool success = base::CreateTemporaryFile(&mhtml_path_);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
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
        base::CreateSequencedTaskRunner(
            {base::ThreadPool(),  // Requires IO.
             base::MayBlock(),

             // TaskPriority: Inherit.

             // Because we are using DELETE_ON_FINAL_RELEASE here, the
             // storage::ScopedFile inside ShareableFileReference requires
             // a shutdown blocking task runner to ensure that its deletion
             // task runs.
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})
            .get());
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnUI,
                     this, success));
}

void PageCaptureSaveAsMHTMLFunction::TemporaryFileCreatedOnUI(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    ReturnFailure(kTemporaryFileError);
    return;
  }

  if (test_delegate_)
    test_delegate_->OnTemporaryFileCreated(mhtml_path_);

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

  error_ = error;

  SendResponse(false);

  // Must not Release() here, OnMessageReceived will call it eventually.
}

void PageCaptureSaveAsMHTMLFunction::ReturnSuccess(int64_t file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = GetWebContents();
  if (!web_contents || !render_frame_host()) {
    ReturnFailure(kTabClosedError);
    return;
  }

  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(source_process_id(),
                                                           mhtml_path_);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("mhtmlFilePath", mhtml_path_.value());
  dict->SetInteger("mhtmlFileLength", file_size);
  SetResult(std::move(dict));

  SendResponse(true);

  // Note that we'll wait for a response ack message received in
  // OnMessageReceived before we call Release() (to prevent the blob file from
  // being deleted).
}

WebContents* PageCaptureSaveAsMHTMLFunction::GetWebContents() {
  Browser* browser = NULL;
  content::WebContents* web_contents = NULL;

  if (!ExtensionTabUtil::GetTabById(params_->details.tab_id, GetProfile(),
                                    include_incognito_information(), &browser,
                                    NULL, &web_contents, NULL)) {
    return NULL;
  }
  return web_contents;
}
