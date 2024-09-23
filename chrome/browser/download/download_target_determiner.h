// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/download/public/common/download_target_info.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_manager_delegate.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#endif

class Profile;
class DownloadPrefs;

// Determines the target of the download.
//
// Terminology:
//   Virtual Path: A path representing the target of the download that may or
//     may not be a physical file path. E.g. if the target of the download is in
//     cloud storage, then the virtual path may be relative to a logical mount
//     point.
//
//   Local Path: A local file system path where the downloads system should
//     write the file to.
//
//   Intermediate Path: Where the data should be written to during the course of
//     the download. Once the download completes, the file could be renamed to
//     Local Path.
//
// DownloadTargetDeterminer is a self owned object that performs the work of
// determining the download target. It observes the DownloadItem and aborts the
// process if the download is removed. DownloadTargetDeterminerDelegate is
// responsible for providing external dependencies and prompting the user if
// necessary.
//
// The only public entrypoint is the static Start() method which creates an
// instance of DownloadTargetDeterminer.
class DownloadTargetDeterminer : public download::DownloadItem::Observer {
 public:
  // A callback to convey the information of the target once determined.
  //
  // |target_info| contains information about the paths, as well as other
  // information about the target.
  //
  // |target_info.danger_type| is set to MAYBE_DANGEROUS_CONTENT if the file
  // type is handled by SafeBrowsing. However, if the SafeBrowsing service is
  // unable to verify whether the file is safe or not, we are on our own. The
  // value of |danger_level| indicates whether the download should be considered
  // dangerous if SafeBrowsing returns an unknown verdict.
  //
  // Note that some downloads (e.g. "Save link as" on a link to a binary) would
  // not be considered 'Dangerous' even if SafeBrowsing came back with an
  // unknown verdict. So we can't always show a warning when SafeBrowsing fails.
  //
  // The value of |danger_level| should be interpreted as follows:
  //
  //   NOT_DANGEROUS : Unless flagged by SafeBrowsing, the file should be
  //       considered safe.
  //
  //   ALLOW_ON_USER_GESTURE : If SafeBrowsing claims the file is safe, then the
  //       file is safe. An UNKOWN verdict results in the file being marked as
  //       DANGEROUS_FILE.
  //
  //   DANGEROUS : This type of file shouldn't be allowed to download without
  //       any user action. Hence, if SafeBrowsing marks the file as SAFE, or
  //       UNKNOWN, the file will still be considered a DANGEROUS_FILE. However,
  //       SafeBrowsing may flag the file as being malicious, in which case the
  //       malicious classification should take precedence.
  using CompletionCallback = base::OnceCallback<void(
      download::DownloadTargetInfo target_info,
      safe_browsing::DownloadFileType::DangerLevel danger_level)>;

  DownloadTargetDeterminer(const DownloadTargetDeterminer&) = delete;
  DownloadTargetDeterminer& operator=(const DownloadTargetDeterminer&) = delete;

  // Start the process of determining the target of |download|.
  //
  // |initial_virtual_path| if non-empty, defines the initial virtual path for
  //   the target determination process. If one isn't specified, one will be
  //   generated based on the response data specified in |download| and the
  //   users' downloads directory.
  //   Note: |initial_virtual_path| is only used if download has prompted the
  //       user before and doesn't have a forced path.
  // |download_prefs| is required and must outlive |download|. It is used for
  //   determining the user's preferences regarding the default downloads
  //   directory, prompting and auto-open behavior.
  // |delegate| is required and must live until |callback| is invoked.
  // |callback| will be scheduled asynchronously on the UI thread after download
  //   determination is complete or after |download| is destroyed.
  //
  // Start() should be called on the UI thread.
  static void Start(
      download::DownloadItem* download,
      const base::FilePath& initial_virtual_path,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action,
      DownloadPrefs* download_prefs,
      DownloadTargetDeterminerDelegate* delegate,
      CompletionCallback callback);

  // Returns a .crdownload intermediate path for the |suggested_path|.
  static base::FilePath GetCrDownloadPath(const base::FilePath& suggested_path);

  // Determine if the file type can be handled safely by the browser if it were
  // to be opened via a file:// URL. Execute the callback with the determined
  // value.
  static void DetermineIfHandledSafelyHelper(
      download::DownloadItem* download,
      const base::FilePath& local_path,
      const std::string& mime_type,
      base::OnceCallback<void(bool)> callback);

  // Determine if the file type can be handled safely by the browser if it were
  // to be opened via a file:// URL. Returns the determined value.
  static bool DetermineIfHandledSafelyHelperSynchronous(
      download::DownloadItem* download,
      const base::FilePath& local_path,
      const std::string& mime_type);

 private:
  // The main workflow is controlled via a set of state transitions. Each state
  // has an associated handler. The handler for STATE_FOO is DoFoo. Each handler
  // performs work, determines the next state to transition to and returns a
  // Result indicating how the workflow should proceed. The loop ends when a
  // handler returns COMPLETE.
  enum State {
    STATE_GENERATE_TARGET_PATH,
    STATE_SET_INSECURE_DOWNLOAD_STATUS,
    STATE_NOTIFY_EXTENSIONS,
    STATE_RESERVE_VIRTUAL_PATH,
    STATE_PROMPT_USER_FOR_DOWNLOAD_PATH,
    STATE_DETERMINE_LOCAL_PATH,
    STATE_DETERMINE_MIME_TYPE,
    STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER,
    STATE_CHECK_DOWNLOAD_URL,
#if BUILDFLAG(IS_ANDROID)
    STATE_CHECK_APP_VERIFICATION,
#endif
    STATE_CHECK_VISITED_REFERRER_BEFORE,
    STATE_DETERMINE_INTERMEDIATE_PATH,
    STATE_NONE,
  };

  // Result code returned by each step of the workflow below. Controls execution
  // of DoLoop().
  enum Result {
    // Continue processing. next_state_ is required to not be STATE_NONE.
    CONTINUE,

    // The DoLoop() that invoked the handler should exit. This value is
    // typically returned when the handler has invoked an asynchronous operation
    // and is expecting a callback. If a handler returns this value, it has
    // taken responsibility for ensuring that DoLoop() is invoked. It is
    // possible that the handler has invoked another DoLoop() already.
    QUIT_DOLOOP,

    // Target determination is complete.
    COMPLETE
  };

  // Used with GetDangerLevel to indicate whether the user has visited the
  // referrer URL for the download prior to today.
  enum PriorVisitsToReferrer {
    NO_VISITS_TO_REFERRER,
    VISITED_REFERRER,
  };

  // Construct a DownloadTargetDeterminer object. Constraints on the arguments
  // are as per Start() above.
  DownloadTargetDeterminer(
      download::DownloadItem* download,
      const base::FilePath& initial_virtual_path,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action,
      DownloadPrefs* download_prefs,
      DownloadTargetDeterminerDelegate* delegate,
      CompletionCallback callback);

  ~DownloadTargetDeterminer() override;

  // Invoke each successive handler until a handler returns QUIT_DOLOOP or
  // COMPLETE. Note that as a result, this object might be deleted. So |this|
  // should not be accessed after calling DoLoop().
  void DoLoop();

  // === Main workflow ===

  // Generates an initial target path. This target is based only on the state of
  // the download item.
  // Next state:
  // - STATE_NONE : If the download is not in progress, returns COMPLETE.
  // - STATE_SET_INSECURE_DOWNLOAD_STATUS : All other downloads.
  Result DoGenerateTargetPath();

  // Determines the insecure download status of the download, so as to block it
  // prior to prompting the user for the file path.  This function relies on the
  // delegate for the actual determination.
  //
  // Next state:
  // - STATE_NOTIFY_EXTENSIONS
  Result DoSetInsecureDownloadStatus();

  // Callback invoked by delegate after insecure download status is determined.
  // Cancels the download if status indicates blocking is necessary.
  void GetInsecureDownloadStatusDone(
      download::DownloadItem::InsecureDownloadStatus status);

  // Notifies downloads extensions. If any extension wishes to override the
  // download filename, it will respond to the OnDeterminingFilename()
  // notification.
  // Next state:
  // - STATE_RESERVE_VIRTUAL_PATH.
  Result DoNotifyExtensions();

  // Callback invoked after extensions are notified. Updates |virtual_path_| and
  // |conflict_action_|.
  void NotifyExtensionsDone(
      const base::FilePath& new_path,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action);

  // Invokes ReserveVirtualPath() on the delegate to acquire a reservation for
  // the path. See DownloadPathReservationTracker.
  // Next state:
  // - STATE_PROMPT_USER_FOR_DOWNLOAD_PATH.
  Result DoReserveVirtualPath();

  // Callback invoked after the delegate aquires a path reservation.
  void ReserveVirtualPathDone(download::PathValidationResult result,
                              const base::FilePath& path);

  // Presents a file picker to the user if necessary.
  // Next state:
  // - STATE_DETERMINE_LOCAL_PATH.
  Result DoRequestConfirmation();

  // Callback invoked after the file picker completes. Cancels the download if
  // the user cancels the file picker.
  void RequestConfirmationDone(DownloadConfirmationResult result,
                               const ui::SelectedFileInfo& selected_file_info);

#if BUILDFLAG(IS_ANDROID)
  // Callback invoked after the incognito message has been accepted/rejected
  // from the user.
  void RequestIncognitoWarningConfirmationDone(bool accepted);
#endif
  // Up until this point, the path that was used is considered to be a virtual
  // path. This step determines the local file system path corresponding to this
  // virtual path. The translation is done by invoking the DetermineLocalPath()
  // method on the delegate.
  // Next state:
  // - STATE_DETERMINE_MIME_TYPE.
  Result DoDetermineLocalPath();

  // Callback invoked when the delegate has determined local path. |file_name|
  // is supplied in case it cannot be determined from local_path (e.g. local
  // path is a content Uri: content://media/12345). |file_name| could be empty
  // if it is the last component of |local_path|.
  void DetermineLocalPathDone(const base::FilePath& local_path,
                              const base::FilePath& file_name);

  // Determine the MIME type corresponding to the local file path. This is only
  // done if the local path and the virtual path was the same. I.e. The file is
  // intended for the local file system. This restriction is there because the
  // resulting MIME type is only valid for determining whether the browser can
  // handle the download if it were opened via a file:// URL.
  // Next state:
  // - STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER.
  Result DoDetermineMimeType();

  // Callback invoked when the MIME type is available. Since determination of
  // the MIME type can involve disk access, it is done in the blocking pool.
  void DetermineMimeTypeDone(const std::string& mime_type);

  // Determine if the file type can be handled safely by the browser if it were
  // to be opened via a file:// URL.
  // Next state:
  // - STATE_CHECK_DOWNLOAD_URL.
  Result DoDetermineIfHandledSafely();

  // Callback invoked when a decision is available about whether the file type
  // can be handled safely by the browser.
  void DetermineIfHandledSafelyDone(bool is_handled_safely);

  // Checks whether the downloaded URL is malicious. Invokes the
  // DownloadProtectionService via the delegate.
  // Next state:
  // - STATE_CHECK_VISITED_REFERRER_BEFORE.
  Result DoCheckDownloadUrl();

  // Callback invoked after the delegate has checked the download URL. Sets the
  // danger type of the download to |danger_type|.
  void CheckDownloadUrlDone(download::DownloadDangerType danger_type);

#if BUILDFLAG(IS_ANDROID)
  // Checks if app verification by Google Play Protect is enabled.
  Result DoCheckAppVerification();

  // Callback invoked after checking if app verification is enabled.
  void CheckAppVerificationDone(safe_browsing::VerifyAppsEnabledResult result);
#endif

  // Checks if the user has visited the referrer URL of the download prior to
  // today. The actual check is only performed if it would be needed to
  // determine the danger type of the download.
  // Next state:
  // - STATE_DETERMINE_INTERMEDIATE_PATH.
  Result DoCheckVisitedReferrerBefore();

  // Callback invoked after completion of history check for prior visits to
  // referrer URL.
  void CheckVisitedReferrerBeforeDone(bool visited_referrer_before);

  // Determines the intermediate path. Once this step completes, downloads
  // target determination is complete. The determination assumes that the
  // intermediate file will never be overwritten (always uniquified if needed).
  // Next state:
  // - STATE_NONE: Returns COMPLETE.
  Result DoDetermineIntermediatePath();

  // === End of main workflow ===

  // Utilities:

  // Schedules the completion callback to be run on the UI thread and deletes
  // this object. The determined target info will be passed into the callback
  // if |interrupt_reason| is NONE. Otherwise, only the interrupt reason will be
  // passed on.
  void ScheduleCallbackAndDeleteSelf(
      download::DownloadInterruptReason interrupt_reason);

  Profile* GetProfile() const;

  // Determine if the download requires confirmation from the user. For regular
  // downloads, this determination is based on the target disposition, auto-open
  // behavior, among other factors. For an interrupted download, this
  // determination will be based on the interrupt reason. It is assumed that
  // download interruptions always occur after the first round of download
  // target determination is complete.
  DownloadConfirmationReason NeedsConfirmation(
      const base::FilePath& filename) const;

  // Returns true if the DLP feature is enabled and downloading the item to
  // `download_path` is blocked, in which case the user should be prompted
  // regardless of the preferences.
  bool IsDownloadDlpBlocked(const base::FilePath& download_path) const;

  // Returns true if the user has been prompted for this download at least once
  // prior to this target determination operation. This method is only expected
  // to return true for a resuming interrupted download that has prompted the
  // user before interruption. The return value does not depend on whether the
  // user will be or has been prompted during the current target determination
  // operation.
  bool HasPromptedForPath() const;

  // Returns true if this download should show the "dangerous file" warning.
  // Various factors are considered, such as the type of the file, whether a
  // user action initiated the download, and whether the user has explicitly
  // marked the file type as "auto open". Protected virtual for testing.
  //
  // If |require_explicit_consent| is non-null then the pointed bool will be set
  // to true if the download requires explicit user consent.
  safe_browsing::DownloadFileType::DangerLevel GetDangerLevel(
      PriorVisitsToReferrer visits) const;

  // Returns the timestamp of the last download bypass.
  std::optional<base::Time> GetLastDownloadBypassTimestamp() const;

  // Generates the download file name based on information from URL, response
  // headers and sniffed mime type.
  base::FilePath GenerateFileName() const;

  // download::DownloadItem::Observer
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // state
  State next_state_;
  DownloadConfirmationReason confirmation_reason_;
  bool should_notify_extensions_;
  bool create_target_directory_;
  download::DownloadPathReservationTracker::FilenameConflictAction
      conflict_action_;
  download::DownloadDangerType danger_type_;
  safe_browsing::DownloadFileType::DangerLevel danger_level_;
  base::FilePath virtual_path_;
  base::FilePath local_path_;
  base::FilePath intermediate_path_;
  std::string mime_type_;
  bool is_filetype_handled_safely_ = false;
  download::DownloadItem::InsecureDownloadStatus insecure_download_status_;
#if BUILDFLAG(IS_ANDROID)
  bool is_checking_dialog_confirmed_path_;
  // Records whether app verification by Play Protect is enabled. When
  // enabled, we suppress warning based only on the file type since Play
  // Protect will give higher quality warnings.
  bool is_app_verification_enabled_;
#endif
#if BUILDFLAG(IS_MAC)
  // A list of tags specified by the user to be set on the file upon the
  // completion of it being written to disk.
  std::vector<std::string> file_tags_;
#endif

  raw_ptr<download::DownloadItem> download_;
  const bool is_resumption_;
  raw_ptr<DownloadPrefs> download_prefs_;
  raw_ptr<DownloadTargetDeterminerDelegate> delegate_;
  CompletionCallback completion_callback_;
  base::CancelableTaskTracker history_tracker_;

  base::WeakPtrFactory<DownloadTargetDeterminer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
