// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_target_determiner.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "net/base/filename_util.h"
#include "net/http/http_content_disposition.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/webstore_installer.h"
#include "extensions/common/feature_switch.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_prefs.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/webplugininfo.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using content::BrowserThread;
using download::DownloadItem;
using download::DownloadPathReservationTracker;
using safe_browsing::DownloadFileType;

namespace {

const base::FilePath::CharType kCrdownloadSuffix[] =
    FILE_PATH_LITERAL(".crdownload");

// Condenses the results from HistoryService::GetVisibleVisitCountToHost() to a
// single bool. A host is considered visited before if prior visible visits were
// found in history and the first such visit was earlier than the most recent
// midnight.
void VisitCountsToVisitedBefore(base::OnceCallback<void(bool)> callback,
                                history::VisibleVisitCountToHostResult result) {
  std::move(callback).Run(
      result.success && result.count > 0 &&
      (result.first_visit.LocalMidnight() < base::Time::Now().LocalMidnight()));
}

#if defined(OS_WIN)
// Keeps track of whether Adobe Reader is up to date.
bool g_is_adobe_reader_up_to_date_ = false;
#endif

}  // namespace

DownloadTargetDeterminerDelegate::~DownloadTargetDeterminerDelegate() {
}

DownloadTargetDeterminer::DownloadTargetDeterminer(
    DownloadItem* download,
    const base::FilePath& initial_virtual_path,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    DownloadPrefs* download_prefs,
    DownloadTargetDeterminerDelegate* delegate,
    const CompletionCallback& callback)
    : next_state_(STATE_GENERATE_TARGET_PATH),
      confirmation_reason_(DownloadConfirmationReason::NONE),
      should_notify_extensions_(false),
      create_target_directory_(false),
      conflict_action_(conflict_action),
      danger_type_(download->GetDangerType()),
      danger_level_(DownloadFileType::NOT_DANGEROUS),
      virtual_path_(initial_virtual_path),
      is_filetype_handled_safely_(false),
#if defined(OS_ANDROID)
      is_checking_dialog_confirmed_path_(false),
#endif
      download_(download),
      is_resumption_(download_->GetLastReason() !=
                         download::DOWNLOAD_INTERRUPT_REASON_NONE &&
                     !initial_virtual_path.empty()),
      download_prefs_(download_prefs),
      delegate_(delegate),
      completion_callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_);
  DCHECK(delegate);
  download_->AddObserver(this);

  DoLoop();
}

DownloadTargetDeterminer::~DownloadTargetDeterminer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_);
  DCHECK(completion_callback_.is_null());
  download_->RemoveObserver(this);
}

void DownloadTargetDeterminer::DoLoop() {
  Result result = CONTINUE;
  do {
    State current_state = next_state_;
    next_state_ = STATE_NONE;

    switch (current_state) {
      case STATE_GENERATE_TARGET_PATH:
        result = DoGenerateTargetPath();
        break;
      case STATE_CHECK_IF_DOWNLOAD_BLOCKED:
        result = DoCheckIfDownloadBlocked();
        break;
      case STATE_NOTIFY_EXTENSIONS:
        result = DoNotifyExtensions();
        break;
      case STATE_RESERVE_VIRTUAL_PATH:
        result = DoReserveVirtualPath();
        break;
      case STATE_PROMPT_USER_FOR_DOWNLOAD_PATH:
        result = DoRequestConfirmation();
        break;
      case STATE_DETERMINE_LOCAL_PATH:
        result = DoDetermineLocalPath();
        break;
      case STATE_DETERMINE_MIME_TYPE:
        result = DoDetermineMimeType();
        break;
      case STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER:
        result = DoDetermineIfHandledSafely();
        break;
      case STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE:
        result = DoDetermineIfAdobeReaderUpToDate();
        break;
      case STATE_CHECK_DOWNLOAD_URL:
        result = DoCheckDownloadUrl();
        break;
      case STATE_DETERMINE_INTERMEDIATE_PATH:
        result = DoDetermineIntermediatePath();
        break;
      case STATE_CHECK_VISITED_REFERRER_BEFORE:
        result = DoCheckVisitedReferrerBefore();
        break;
      case STATE_NONE:
        NOTREACHED();
        return;
    }
  } while (result == CONTINUE);
  // Note that if a callback completes synchronously, the handler will still
  // return QUIT_DOLOOP. In this case, an inner DoLoop() may complete the target
  // determination and delete |this|.

  if (result == COMPLETE)
    ScheduleCallbackAndDeleteSelf(download::DOWNLOAD_INTERRUPT_REASON_NONE);
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoGenerateTargetPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(local_path_.empty());
  DCHECK_EQ(confirmation_reason_, DownloadConfirmationReason::NONE);
  DCHECK(!should_notify_extensions_);
  bool is_forced_path = !download_->GetForcedFilePath().empty();

  next_state_ = STATE_CHECK_IF_DOWNLOAD_BLOCKED;

  // Transient download should use the existing path.
  if (download_->IsTransient()) {
    if (is_forced_path) {
      RecordDownloadPathGeneration(DownloadPathGenerationEvent::USE_FORCE_PATH,
                                   true);
      virtual_path_ = download_->GetForcedFilePath();
    } else if (!virtual_path_.empty()) {
      RecordDownloadPathGeneration(
          DownloadPathGenerationEvent::USE_EXISTING_VIRTUAL_PATH, true);
    } else {
      // No path is provided, we have no idea what the target path is. Stop the
      // target determination process and wait for self deletion.
      RecordDownloadPathGeneration(DownloadPathGenerationEvent::NO_VALID_PATH,
                                   true);
      ScheduleCallbackAndDeleteSelf(
          download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
      return QUIT_DOLOOP;
    }

    conflict_action_ = DownloadPathReservationTracker::OVERWRITE;
    DCHECK(virtual_path_.IsAbsolute());
    return CONTINUE;
  }

  bool no_prompt_needed = HasPromptedForPath();
#if defined(OS_ANDROID)
  // If |virtual_path_| is content URI, there is no need to prompt the user.
  no_prompt_needed |= virtual_path_.IsContentUri();
#endif
  if (!virtual_path_.empty() && no_prompt_needed && !is_forced_path) {
    // The download is being resumed and the user has already been prompted for
    // a path. Assume that it's okay to overwrite the file if there's a conflict
    // and reuse the selection.
    confirmation_reason_ = NeedsConfirmation(virtual_path_);
    conflict_action_ = DownloadPathReservationTracker::OVERWRITE;
    RecordDownloadPathGeneration(
        DownloadPathGenerationEvent::USE_EXISTING_VIRTUAL_PATH, false);
  } else if (!is_forced_path) {
    // If we don't have a forced path, we should construct a path for the
    // download. Forced paths are only specified for programmatic downloads
    // (WebStore, Drag&Drop). Treat the path as a virtual path. We will
    // eventually determine whether this is a local path and if not, figure out
    // a local path.
    std::string suggested_filename = download_->GetSuggestedFilename();
    if (suggested_filename.empty() &&
        download_->GetMimeType() == "application/x-x509-user-cert") {
      suggested_filename = "user.crt";
    }

    std::string default_filename(
        l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME));
    std::string referrer_charset =
        GetProfile()->GetPrefs()->GetString(prefs::kDefaultCharset);
    base::FilePath generated_filename = net::GenerateFileName(
        download_->GetURL(), download_->GetContentDisposition(),
        referrer_charset, suggested_filename, download_->GetMimeType(),
        default_filename);

    // Replace the file extension based on the mime type informed by
    // Content-Type header if the file extension generated by the URL is
    // considered safe by safe browsing. Noticed that when generating file name,
    // |suggested_filename| and Content-Disposition header have higher priority
    // than the URL.
    bool safe_file_ext =
        !safe_browsing::FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
            generated_filename);
    net::HttpContentDisposition content_disposition_header(
        download_->GetContentDisposition(), referrer_charset);
    bool should_replace_extension =
        safe_file_ext && !download_->GetMimeType().empty() &&
        suggested_filename.empty() &&
        content_disposition_header.filename().empty();
    if (should_replace_extension) {
      generated_filename = net::GenerateFileName(
          download_->GetURL(), std::string() /* content_disposition */,
          referrer_charset, std::string() /* suggested_filename */,
          download_->GetMimeType(), default_filename,
          true /* should_replace_extension */);
    }

    confirmation_reason_ = NeedsConfirmation(generated_filename);
    base::FilePath target_directory;
    if (confirmation_reason_ != DownloadConfirmationReason::NONE) {
      DCHECK(!download_prefs_->IsDownloadPathManaged());
      // If the user is going to be prompted and the user has been prompted
      // before, then always prefer the last directory that the user selected.
      target_directory = download_prefs_->SaveFilePath();
      RecordDownloadPathGeneration(
          DownloadPathGenerationEvent::USE_LAST_PROMPT_DIRECTORY, false);
    } else {
      target_directory = download_prefs_->DownloadPath();
      RecordDownloadPathGeneration(
          DownloadPathGenerationEvent::USE_DEFAULTL_DOWNLOAD_DIRECTORY, false);
    }
    virtual_path_ = target_directory.Append(generated_filename);
    should_notify_extensions_ = true;
    DCHECK(virtual_path_.IsAbsolute());
  } else {
    conflict_action_ = DownloadPathReservationTracker::OVERWRITE;
    virtual_path_ = download_->GetForcedFilePath();
    RecordDownloadPathGeneration(DownloadPathGenerationEvent::USE_FORCE_PATH,
                                 false);
    // If this is a resumed download which was previously interrupted due to an
    // issue with the forced path, the user is still not prompted. If the path
    // supplied to a programmatic download is invalid, then the caller needs to
    // intervene.
    DCHECK(virtual_path_.IsAbsolute());
  }
  DVLOG(20) << "Generated virtual path: " << virtual_path_.AsUTF8Unsafe();

  return CONTINUE;
}

DownloadTargetDeterminer::Result
DownloadTargetDeterminer::DoCheckIfDownloadBlocked() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_NOTIFY_EXTENSIONS;

  delegate_->ShouldBlockDownload(
      download_, virtual_path_,
      base::Bind(&DownloadTargetDeterminer::CheckIfDownloadBlockedDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::CheckIfDownloadBlockedDone(bool should_block) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Delegate should not call back here more than once.
  DCHECK_EQ(STATE_NOTIFY_EXTENSIONS, next_state_);

  if (should_block) {
    ScheduleCallbackAndDeleteSelf(
        download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
    return;
  }

  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoNotifyExtensions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_RESERVE_VIRTUAL_PATH;

  if (!should_notify_extensions_ ||
      download_->GetState() != DownloadItem::IN_PROGRESS)
    return CONTINUE;

  delegate_->NotifyExtensions(download_, virtual_path_,
      base::Bind(&DownloadTargetDeterminer::NotifyExtensionsDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::NotifyExtensionsDone(
    const base::FilePath& suggested_path,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Extension suggested path: " << suggested_path.AsUTF8Unsafe();

  // Extensions should not call back here more than once.
  DCHECK_EQ(STATE_RESERVE_VIRTUAL_PATH, next_state_);

  if (!suggested_path.empty()) {
    // If an extension overrides the filename, then the target directory will be
    // forced to download_prefs_->DownloadPath() since extensions cannot place
    // downloaded files anywhere except there. This prevents subdirectories from
    // accumulating: if an extension is allowed to say that a file should go in
    // last_download_path/music/foo.mp3, then last_download_path will accumulate
    // the subdirectory /music/ so that the next download may end up in
    // Downloads/music/music/music/bar.mp3.
    base::FilePath new_path(download_prefs_->DownloadPath().Append(
        suggested_path).NormalizePathSeparators());
    // If the (Chrome) extension does not suggest an file extension, do not pass
    // a mime type to GenerateSafeFileName so that it does not force the
    // filename to have an extension. Otherwise, correct the file extension in
    // case it is wrongly given.
    if (new_path.Extension().empty())
      net::GenerateSafeFileName(std::string(), false, &new_path);
    else
      net::GenerateSafeFileName(download_->GetMimeType(), true, &new_path);
    virtual_path_ = new_path;
    create_target_directory_ = true;
  }
  // An extension may set conflictAction without setting filename.
  if (conflict_action != DownloadPathReservationTracker::UNIQUIFY)
    conflict_action_ = conflict_action;

  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoReserveVirtualPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());

  next_state_ = STATE_PROMPT_USER_FOR_DOWNLOAD_PATH;
  if (download_->GetState() != DownloadItem::IN_PROGRESS)
    return CONTINUE;

  delegate_->ReserveVirtualPath(
      download_, virtual_path_, create_target_directory_, conflict_action_,
      base::Bind(&DownloadTargetDeterminer::ReserveVirtualPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::ReserveVirtualPathDone(
    download::PathValidationResult result,
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(20) << "Reserved path: " << path.AsUTF8Unsafe()
            << " Result:" << static_cast<int>(result);
  DCHECK_EQ(STATE_PROMPT_USER_FOR_DOWNLOAD_PATH, next_state_);
  RecordDownloadPathValidation(result, download_->IsTransient());
  if (download_->IsTransient()) {
    DCHECK_EQ(DownloadConfirmationReason::NONE, confirmation_reason_)
        << "Transient download should not ask the user for confirmation.";
    DCHECK(result != download::PathValidationResult::CONFLICT)
        << "Transient download"
           "should always overwrite the file.";
    switch (result) {
      case download::PathValidationResult::PATH_NOT_WRITABLE:
      case download::PathValidationResult::NAME_TOO_LONG:
      case download::PathValidationResult::CONFLICT:
        ScheduleCallbackAndDeleteSelf(
            download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
        return;
      case download::PathValidationResult::SUCCESS:
      case download::PathValidationResult::SAME_AS_SOURCE:
        DCHECK_EQ(virtual_path_, path) << "Transient download path should not"
                                          "be changed.";
        break;
      case download::PathValidationResult::COUNT:
        NOTREACHED();
    }
  } else {
    virtual_path_ = path;

    switch (result) {
      case download::PathValidationResult::SUCCESS:
      case download::PathValidationResult::SAME_AS_SOURCE:
        break;

      case download::PathValidationResult::PATH_NOT_WRITABLE:
        confirmation_reason_ =
            DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE;
        break;

      case download::PathValidationResult::NAME_TOO_LONG:
        confirmation_reason_ = DownloadConfirmationReason::NAME_TOO_LONG;
        break;

      case download::PathValidationResult::CONFLICT:
        confirmation_reason_ = DownloadConfirmationReason::TARGET_CONFLICT;
        break;
      case download::PathValidationResult::COUNT:
        NOTREACHED();
    }
  }

  DoLoop();
}

DownloadTargetDeterminer::Result
DownloadTargetDeterminer::DoRequestConfirmation() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!download_->IsTransient() ||
         confirmation_reason_ == DownloadConfirmationReason::NONE);

  next_state_ = STATE_DETERMINE_LOCAL_PATH;

  // Avoid prompting for a download if it isn't in-progress. The user will be
  // prompted once the download is resumed and headers are available.
  if (download_->GetState() == DownloadItem::IN_PROGRESS) {
#if defined(OS_ANDROID)
    // If we were looping back to check the user-confirmed path from the
    // dialog, and there were no additional errors, continue.
    if (is_checking_dialog_confirmed_path_ &&
        (confirmation_reason_ == DownloadConfirmationReason::PREFERENCE ||
         confirmation_reason_ == DownloadConfirmationReason::NONE)) {
      is_checking_dialog_confirmed_path_ = false;
      return CONTINUE;
    }
#endif

    // If there is a non-neutral confirmation reason, prompt the user.
    if (confirmation_reason_ != DownloadConfirmationReason::NONE) {
      delegate_->RequestConfirmation(
          download_, virtual_path_, confirmation_reason_,
          base::BindRepeating(
              &DownloadTargetDeterminer::RequestConfirmationDone,
              weak_ptr_factory_.GetWeakPtr()));
      return QUIT_DOLOOP;
    }
  }

  return CONTINUE;
}

void DownloadTargetDeterminer::RequestConfirmationDone(
    DownloadConfirmationResult result,
    const base::FilePath& virtual_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!download_->IsTransient());
  DVLOG(20) << "User selected path:" << virtual_path.AsUTF8Unsafe();
#if defined(OS_ANDROID)
  is_checking_dialog_confirmed_path_ = false;
#endif
  if (result == DownloadConfirmationResult::CANCELED) {
    ScheduleCallbackAndDeleteSelf(
        download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
    return;
  }
  DCHECK(!virtual_path.empty());
  DCHECK_EQ(STATE_DETERMINE_LOCAL_PATH, next_state_);

  // If the user wasn't prompted, then we need to clear the
  // confirmation_reason_. This way it's clear that user has not given consent
  // to download this resource.
  if (result == DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION)
    confirmation_reason_ = DownloadConfirmationReason::NONE;

  virtual_path_ = virtual_path;

#if defined(OS_ANDROID)
  if (result == DownloadConfirmationResult::CONFIRMED_WITH_DIALOG) {
    // Double check the user-selected path is valid by looping back.
    is_checking_dialog_confirmed_path_ = true;
    confirmation_reason_ = DownloadConfirmationReason::NONE;
    next_state_ = STATE_RESERVE_VIRTUAL_PATH;
  }
#endif

  download_prefs_->SetSaveFilePath(virtual_path_.DirName());
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineLocalPath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(local_path_.empty());

  next_state_ = STATE_DETERMINE_MIME_TYPE;

  delegate_->DetermineLocalPath(
      download_,
      virtual_path_,
      base::Bind(&DownloadTargetDeterminer::DetermineLocalPathDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::DetermineLocalPathDone(
    const base::FilePath& local_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Local path: " << local_path.AsUTF8Unsafe();
  if (local_path.empty()) {
    // Path subsitution failed. Usually caused by something going wrong with the
    // Google Drive logic (e.g. filesystem error while trying to create the
    // cache file). We are going to return a generic error here since a more
    // specific one is unlikely to be helpful to the user.
    ScheduleCallbackAndDeleteSelf(
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
    return;
  }
  DCHECK_EQ(STATE_DETERMINE_MIME_TYPE, next_state_);

  local_path_ = local_path;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineMimeType() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(mime_type_.empty());

  next_state_ = STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER;

  if (virtual_path_ == local_path_) {
    delegate_->GetFileMimeType(
        local_path_,
        base::Bind(&DownloadTargetDeterminer::DetermineMimeTypeDone,
                   weak_ptr_factory_.GetWeakPtr()));
    return QUIT_DOLOOP;
  }
  return CONTINUE;
}

void DownloadTargetDeterminer::DetermineMimeTypeDone(
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "MIME type: " << mime_type;
  DCHECK_EQ(STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER, next_state_);

  mime_type_ = mime_type;
  DoLoop();
}

#if BUILDFLAG(ENABLE_PLUGINS)
// The code below is used by DoDetermineIfHandledSafely to determine if the
// file type is handled by a sandboxed plugin.
namespace {

void InvokeClosureAfterGetPluginCallback(
    const base::Closure& closure,
    const std::vector<content::WebPluginInfo>& unused) {
  closure.Run();
}

enum ActionOnStalePluginList {
  RETRY_IF_STALE_PLUGIN_LIST,
  IGNORE_IF_STALE_PLUGIN_LIST
};

void IsHandledBySafePlugin(int render_process_id,
                           int routing_id,
                           const GURL& url,
                           const std::string& mime_type,
                           ActionOnStalePluginList stale_plugin_action,
                           const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!mime_type.empty());
  using content::WebPluginInfo;

  std::string actual_mime_type;
  bool is_stale = false;
  WebPluginInfo plugin_info;

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  bool plugin_found = plugin_service->GetPluginInfo(
      render_process_id, routing_id, url, url::Origin(), mime_type, false,
      &is_stale, &plugin_info, &actual_mime_type);
  if (is_stale && stale_plugin_action == RETRY_IF_STALE_PLUGIN_LIST) {
    // The GetPlugins call causes the plugin list to be refreshed. Once that's
    // done we can retry the GetPluginInfo call. We break out of this cycle
    // after a single retry in order to avoid retrying indefinitely.
    plugin_service->GetPlugins(base::BindOnce(
        &InvokeClosureAfterGetPluginCallback,
        base::Bind(&IsHandledBySafePlugin, render_process_id, routing_id, url,
                   mime_type, IGNORE_IF_STALE_PLUGIN_LIST, callback)));
    return;
  }
  // In practice, we assume that retrying once is enough.
  DCHECK(!is_stale);
  bool is_handled_safely =
      plugin_found &&
      (plugin_info.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS ||
       plugin_info.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS ||
       plugin_info.type == WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(callback, is_handled_safely));
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_PLUGINS)

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIfHandledSafely() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(!is_filetype_handled_safely_);

  next_state_ = STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE;

  if (mime_type_.empty())
    return CONTINUE;

  if (blink::IsSupportedMimeType(mime_type_)) {
    is_filetype_handled_safely_ = true;
    return CONTINUE;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  int render_process_id = -1;
  int routing_id = -1;
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(download_);
  if (web_contents) {
    render_process_id = web_contents->GetMainFrame()->GetProcess()->GetID();
    routing_id = web_contents->GetMainFrame()->GetRoutingID();
  }
  IsHandledBySafePlugin(
      render_process_id, routing_id, net::FilePathToFileURL(local_path_),
      mime_type_, RETRY_IF_STALE_PLUGIN_LIST,
      base::Bind(&DownloadTargetDeterminer::DetermineIfHandledSafelyDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
#else
  return CONTINUE;
#endif
}

#if BUILDFLAG(ENABLE_PLUGINS)
void DownloadTargetDeterminer::DetermineIfHandledSafelyDone(
    bool is_handled_safely) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Is file type handled safely: " << is_filetype_handled_safely_;
  DCHECK_EQ(STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE, next_state_);
  is_filetype_handled_safely_ = is_handled_safely;
  DoLoop();
}
#endif

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIfAdobeReaderUpToDate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  next_state_ = STATE_CHECK_DOWNLOAD_URL;

#if defined(OS_WIN)
  if (!local_path_.MatchesExtension(FILE_PATH_LITERAL(".pdf")))
    return CONTINUE;
  if (!IsAdobeReaderDefaultPDFViewer()) {
    g_is_adobe_reader_up_to_date_ = false;
    return CONTINUE;
  }

  // IsAdobeReaderUpToDate() needs to be run with COM as it makes COM calls via
  // AssocQueryString() in IsAdobeReaderDefaultPDFViewer().
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock()})
          .get(),
      FROM_HERE, base::Bind(&::IsAdobeReaderUpToDate),
      base::Bind(&DownloadTargetDeterminer::DetermineIfAdobeReaderUpToDateDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
#else
  return CONTINUE;
#endif
}

#if defined(OS_WIN)
void DownloadTargetDeterminer::DetermineIfAdobeReaderUpToDateDone(
    bool adobe_reader_up_to_date) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "Is Adobe Reader Up To Date: " << adobe_reader_up_to_date;
  DCHECK_EQ(STATE_CHECK_DOWNLOAD_URL, next_state_);
  g_is_adobe_reader_up_to_date_ = adobe_reader_up_to_date;
  DoLoop();
}
#endif

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckDownloadUrl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  next_state_ = STATE_CHECK_VISITED_REFERRER_BEFORE;

  // If user has validated a dangerous download, don't check.
  if (danger_type_ == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED)
    return CONTINUE;

  delegate_->CheckDownloadUrl(
      download_,
      virtual_path_,
      base::Bind(&DownloadTargetDeterminer::CheckDownloadUrlDone,
                 weak_ptr_factory_.GetWeakPtr()));
  return QUIT_DOLOOP;
}

void DownloadTargetDeterminer::CheckDownloadUrlDone(
    download::DownloadDangerType danger_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(20) << "URL Check Result:" << danger_type;
  DCHECK_EQ(STATE_CHECK_VISITED_REFERRER_BEFORE, next_state_);
  danger_type_ = danger_type;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoCheckVisitedReferrerBefore() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  next_state_ = STATE_DETERMINE_INTERMEDIATE_PATH;

  // Checking if there are prior visits to the referrer is only necessary if the
  // danger level of the download depends on the file type.
  if (danger_type_ != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS &&
      danger_type_ != download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT &&
      danger_type_ != download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY) {
    return CONTINUE;
  }

  // First determine the danger level assuming that the user doesn't have any
  // prior visits to the referrer recoreded in history. The resulting danger
  // level would be ALLOW_ON_USER_GESTURE if the level depends on the visit
  // history. In the latter case, we can query the history DB to determine if
  // there were prior requests and determine the danger level again once the
  // result is available.
  danger_level_ = GetDangerLevel(NO_VISITS_TO_REFERRER);

  if (danger_level_ == DownloadFileType::NOT_DANGEROUS)
    return CONTINUE;

  if (danger_level_ == DownloadFileType::ALLOW_ON_USER_GESTURE) {
    // HistoryServiceFactory redirects incognito profiles to on-record profiles.
    // There's no history for on-record profiles in unit_tests.
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

    if (history_service && download_->GetReferrerUrl().is_valid()) {
      history_service->GetVisibleVisitCountToHost(
          download_->GetReferrerUrl(),
          base::BindOnce(
              &VisitCountsToVisitedBefore,
              base::BindOnce(
                  &DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone,
                  weak_ptr_factory_.GetWeakPtr())),
          &history_tracker_);
      return QUIT_DOLOOP;
    }
  }

  // If the danger level doesn't depend on having visited the refererrer URL or
  // if original profile doesn't have a HistoryService or the referrer url is
  // invalid, then assume the referrer has not been visited before.
  if (danger_type_ == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    danger_type_ = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  return CONTINUE;
}

void DownloadTargetDeterminer::CheckVisitedReferrerBeforeDone(
    bool visited_referrer_before) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(STATE_DETERMINE_INTERMEDIATE_PATH, next_state_);
  danger_level_ = GetDangerLevel(
      visited_referrer_before ? VISITED_REFERRER : NO_VISITS_TO_REFERRER);
  if (danger_level_ != DownloadFileType::NOT_DANGEROUS &&
      danger_type_ == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)
    danger_type_ = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  DoLoop();
}

DownloadTargetDeterminer::Result
    DownloadTargetDeterminer::DoDetermineIntermediatePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!virtual_path_.empty());
  DCHECK(!local_path_.empty());
  DCHECK(intermediate_path_.empty());
  DCHECK(!virtual_path_.MatchesExtension(kCrdownloadSuffix));
  DCHECK(!local_path_.MatchesExtension(kCrdownloadSuffix));

  next_state_ = STATE_NONE;

#if defined(OS_ANDROID)
  // If the local path is a content URI, the download should be from resumption
  // and we can just use the current path.
  if (local_path_.IsContentUri()) {
    DCHECK(is_resumption_);
    intermediate_path_ = local_path_;
    return COMPLETE;
  }
#endif

  // Note that the intermediate filename is always uniquified (i.e. if a file by
  // the same name exists, it is never overwritten). Therefore the code below
  // does not attempt to find a name that doesn't conflict with an existing
  // file.

  // If the actual target of the download is a virtual path, then the local path
  // is considered to point to a temporary path. A separate intermediate path is
  // unnecessary since the local path already serves that purpose.
  if (virtual_path_.BaseName() != local_path_.BaseName()) {
    intermediate_path_ = local_path_;
    return COMPLETE;
  }

  // If the download has a forced path and is safe, then just use the
  // target path. In practice the temporary download file that was created prior
  // to download filename determination is already named
  // download_->GetForcedFilePath().
  if (danger_type_ == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS &&
      !download_->GetForcedFilePath().empty()) {
    DCHECK_EQ(download_->GetForcedFilePath().value(), local_path_.value());
    intermediate_path_ = local_path_;
    return COMPLETE;
  }

  // Transient downloads don't need to be renamed to intermediate file.
  if (danger_type_ == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS &&
      download_->IsTransient()) {
    intermediate_path_ = local_path_;
    return COMPLETE;
  }

  // Other safe downloads get a .crdownload suffix for their intermediate name.
  if (danger_type_ == download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    intermediate_path_ = GetCrDownloadPath(local_path_);
    return COMPLETE;
  }

  // If this is a resumed download, then re-use the existing intermediate path
  // if one is available. A resumed download shouldn't cause a non-dangerous
  // download to be considered dangerous upon resumption. Therefore the
  // intermediate file should already be in the correct form.
  if (is_resumption_ && !download_->GetFullPath().empty() &&
      local_path_.DirName() == download_->GetFullPath().DirName()) {
    DCHECK_NE(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
              download_->GetDangerType());
    DCHECK_EQ(kCrdownloadSuffix, download_->GetFullPath().Extension());
    intermediate_path_ = download_->GetFullPath();
    return COMPLETE;
  }

  // Dangerous downloads receive a random intermediate name that looks like:
  // 'Unconfirmed <random>.crdownload'.
  const base::FilePath::CharType kUnconfirmedFormatSuffix[] =
      FILE_PATH_LITERAL(" %d.crdownload");
  // Range of the <random> uniquifier.
  const int kUnconfirmedUniquifierRange = 1000000;
#if defined(OS_WIN)
  base::string16 unconfirmed_format =
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#else
  std::string unconfirmed_format =
      l10n_util::GetStringUTF8(IDS_DOWNLOAD_UNCONFIRMED_PREFIX);
#endif
  unconfirmed_format.append(kUnconfirmedFormatSuffix);

  base::FilePath::StringType file_name = base::StringPrintf(
      unconfirmed_format.c_str(),
      base::RandInt(0, kUnconfirmedUniquifierRange));
  intermediate_path_ = local_path_.DirName().Append(file_name);
  return COMPLETE;
}

void DownloadTargetDeterminer::ScheduleCallbackAndDeleteSelf(
    download::DownloadInterruptReason result) {
  DCHECK(download_);
  DVLOG(20) << "Scheduling callback. Virtual:" << virtual_path_.AsUTF8Unsafe()
            << " Local:" << local_path_.AsUTF8Unsafe()
            << " Intermediate:" << intermediate_path_.AsUTF8Unsafe()
            << " Confirmation reason:" << static_cast<int>(confirmation_reason_)
            << " Danger type:" << danger_type_
            << " Danger level:" << danger_level_
            << " Result:" << static_cast<int>(result);
  std::unique_ptr<DownloadTargetInfo> target_info(new DownloadTargetInfo);

  target_info->target_path = local_path_;
  target_info->result = result;
  target_info->target_disposition =
      (HasPromptedForPath() ||
               confirmation_reason_ != DownloadConfirmationReason::NONE
           ? DownloadItem::TARGET_DISPOSITION_PROMPT
           : DownloadItem::TARGET_DISPOSITION_OVERWRITE);
  target_info->danger_type = danger_type_;
  target_info->danger_level = danger_level_;
  target_info->intermediate_path = intermediate_path_;
  target_info->mime_type = mime_type_;
  target_info->is_filetype_handled_safely = is_filetype_handled_safely_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(completion_callback_, std::move(target_info)));
  completion_callback_.Reset();
  delete this;
}

Profile* DownloadTargetDeterminer::GetProfile() const {
  DCHECK(content::DownloadItemUtils::GetBrowserContext(download_));
  return Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_));
}

DownloadConfirmationReason DownloadTargetDeterminer::NeedsConfirmation(
    const base::FilePath& filename) const {
  // Transient download never has user interaction.
  if (download_->IsTransient())
    return DownloadConfirmationReason::NONE;

  if (is_resumption_) {
    // For resumed downloads, if the target disposition or prefs require
    // prompting, the user has already been prompted. Try to respect the user's
    // selection, unless we've discovered that the target path cannot be used
    // for some reason.
    download::DownloadInterruptReason reason = download_->GetLastReason();
    switch (reason) {
      case download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
        return DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE;

      case download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      case download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
        return DownloadConfirmationReason::TARGET_NO_SPACE;

      default:
        return DownloadConfirmationReason::NONE;
    }
  }

  // If the download path is forced, don't prompt.
  if (!download_->GetForcedFilePath().empty()) {
    // 'Save As' downloads shouldn't have a forced path.
    DCHECK(DownloadItem::TARGET_DISPOSITION_PROMPT !=
           download_->GetTargetDisposition());
    return DownloadConfirmationReason::NONE;
  }

  // Don't ask where to save if the download path is managed. Even if the user
  // wanted to be prompted for "all" downloads, or if this was a 'Save As'
  // download.
  if (download_prefs_->IsDownloadPathManaged())
    return DownloadConfirmationReason::NONE;

  // Prompt if this is a 'Save As' download.
  if (download_->GetTargetDisposition() ==
      DownloadItem::TARGET_DISPOSITION_PROMPT)
    return DownloadConfirmationReason::SAVE_AS;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Don't prompt for extension downloads if the installation site is white
  // listed.
  if (download_crx_util::IsTrustedExtensionDownload(GetProfile(), *download_))
    return DownloadConfirmationReason::NONE;
#endif

  // Don't prompt for file types that are marked for opening automatically.
  if (download_prefs_->IsAutoOpenEnabledBasedOnExtension(filename))
    return DownloadConfirmationReason::NONE;

  // For everything else, prompting is controlled by the PromptForDownload pref.
  // The user may still be prompted even if this pref is disabled due to, for
  // example, there being an unresolvable filename conflict or the target path
  // is not writeable.
  return download_prefs_->PromptForDownload()
             ? DownloadConfirmationReason::PREFERENCE
             : DownloadConfirmationReason::NONE;
}

bool DownloadTargetDeterminer::HasPromptedForPath() const {
  return (is_resumption_ && download_->GetTargetDisposition() ==
                                DownloadItem::TARGET_DISPOSITION_PROMPT);
}

DownloadFileType::DangerLevel DownloadTargetDeterminer::GetDangerLevel(
    PriorVisitsToReferrer visits) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the user has has been prompted or will be, assume that the user has
  // approved the download. A programmatic download is considered safe unless it
  // contains malware.
  if (HasPromptedForPath() ||
      confirmation_reason_ != DownloadConfirmationReason::NONE ||
      !download_->GetForcedFilePath().empty())
    return DownloadFileType::NOT_DANGEROUS;

  // User-initiated extension downloads from pref-whitelisted sources are not
  // considered dangerous.
  if (download_->HasUserGesture() &&
      download_crx_util::IsTrustedExtensionDownload(GetProfile(), *download_)) {
    return DownloadFileType::NOT_DANGEROUS;
  }

  // Anything the user has marked auto-open is OK if it's user-initiated.
  if (download_prefs_->IsAutoOpenEnabledBasedOnExtension(virtual_path_) &&
      download_->HasUserGesture())
    return DownloadFileType::NOT_DANGEROUS;

  DownloadFileType::DangerLevel danger_level =
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          virtual_path_.BaseName());

  // A danger level of ALLOW_ON_USER_GESTURE is used to label potentially
  // dangerous file types that have a high frequency of legitimate use. We would
  // like to avoid prompting for the legitimate cases as much as possible. To
  // that end, we consider a download to be legitimate if one of the following
  // is true, and avoid prompting:
  //
  // * The user navigated to the download URL via the omnibox (either by typing
  //   the URL, pasting it, or using search).
  //
  // * The navigation that initiated the download has a user gesture associated
  //   with it AND the user the user is familiar with the referring origin. A
  //   user is considered familiar with a referring origin if a visit for a page
  //   from the same origin was recorded on the previous day or earlier.
  if (danger_level == DownloadFileType::ALLOW_ON_USER_GESTURE &&
      ((download_->GetTransitionType() &
        ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0 ||
       (download_->HasUserGesture() && visits == VISITED_REFERRER)))
    return DownloadFileType::NOT_DANGEROUS;
  return danger_level;
}

void DownloadTargetDeterminer::OnDownloadDestroyed(
    DownloadItem* download) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(download_, download);
  ScheduleCallbackAndDeleteSelf(
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
}

// static
void DownloadTargetDeterminer::Start(
    download::DownloadItem* download,
    const base::FilePath& initial_virtual_path,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    DownloadPrefs* download_prefs,
    DownloadTargetDeterminerDelegate* delegate,
    const CompletionCallback& callback) {
  // DownloadTargetDeterminer owns itself and will self destruct when the job is
  // complete or the download item is destroyed. The callback is always invoked
  // asynchronously.
  new DownloadTargetDeterminer(download, initial_virtual_path, conflict_action,
                               download_prefs, delegate, callback);
}

// static
base::FilePath DownloadTargetDeterminer::GetCrDownloadPath(
    const base::FilePath& suggested_path) {
  return base::FilePath(suggested_path.value() + kCrdownloadSuffix);
}

#if defined(OS_WIN)
// static
bool DownloadTargetDeterminer::IsAdobeReaderUpToDate() {
  return g_is_adobe_reader_up_to_date_;
}
#endif
