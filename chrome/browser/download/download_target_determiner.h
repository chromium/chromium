// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/common/safe_browsing/download_file_types.pb.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "content/public/browser/download_manager_delegate.h"
#include "ppapi/buildflags/buildflags.h"

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
  using CompletionCallback =
      base::Callback<void(std::unique_ptr<DownloadTargetInfo>)>;

  // Start the process of determing the target of |download|.
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
      const CompletionCallback& callback);

  // Returns a .crdownload intermediate path for the |suggested_path|.
  static base::FilePath GetCrDownloadPath(const base::FilePath& suggested_path);

#if defined(OS_WIN)
  // Returns true if Adobe Reader is up to date. This information refreshed
  // only when Start() gets called for a PDF and Adobe Reader is the default
  // System PDF viewer.
  static bool IsAdobeReaderUpToDate();
#endif

 private:
  // The main workflow is controlled via a set of state transitions. Each state
  // has an associated handler. The handler for STATE_FOO is DoFoo. Each handler
  // performs work, determines the next state to transition to and returns a
  // Result indicating how the workflow should proceed. The loop ends when a
  // handler returns COMPLETE.
  enum State {
    STATE_GENERATE_TARGET_PATH,
    STATE_CHECK_IF_DOWNLOAD_BLOCKED,
    STATE_NOTIFY_EXTENSIONS,
    STATE_RESERVE_VIRTUAL_PATH,
    STATE_PROMPT_USER_FOR_DOWNLOAD_PATH,
    STATE_DETERMINE_LOCAL_PATH,
    STATE_DETERMINE_MIME_TYPE,
    STATE_DETERMINE_IF_HANDLED_SAFELY_BY_BROWSER,
    STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE,
    STATE_CHECK_DOWNLOAD_URL,
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
      const CompletionCallback& callback);

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
  // - STATE_CHECK_IF_DOWNLOAD_BLOCKED : All other downloads.
  Result DoGenerateTargetPath();

  // Determines whether the download ought to be blocked before a user is
  // prompted for file path. Used for active mixed content blocking. This
  // function relies on the delegate for the actual determination.
  //
  // Next state:
  // - STATE_NOTIFY_EXTENSIONS
  Result DoCheckIfDownloadBlocked();

  // Callback invoked by delegate after blocking is determined. Does the actual
  // cancellation of the download if necessary.
  void CheckIfDownloadBlockedDone(bool should_block);

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
                               const base::FilePath& virtual_path);

  // Up until this point, the path that was used is considered to be a virtual
  // path. This step determines the local file system path corresponding to this
  // virtual path. The translation is done by invoking the DetermineLocalPath()
  // method on the delegate.
  // Next state:
  // - STATE_DETERMINE_MIME_TYPE.
  Result DoDetermineLocalPath();

  // Callback invoked when the delegate has determined local path.
  void DetermineLocalPathDone(const base::FilePath& local_path);

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
  // - STATE_DETERMINE_IF_ADOBE_READER_UP_TO_DATE.
  Result DoDetermineIfHandledSafely();

#if BUILDFLAG(ENABLE_PLUGINS)
  // Callback invoked when a decision is available about whether the file type
  // can be handled safely by the browser.
  void DetermineIfHandledSafelyDone(bool is_handled_safely);
#endif

  // Determine if Adobe Reader is up to date. Only do the check on Windows for
  // .pdf file targets.
  // Next state:
  // - STATE_CHECK_DOWNLOAD_URL.
  Result DoDetermineIfAdobeReaderUpToDate();

#if defined(OS_WIN)
  // Callback invoked when a decision is available about whether Adobe Reader
  // is up to date.
  void DetermineIfAdobeReaderUpToDateDone(bool adobe_reader_up_to_date);
#endif

  // Checks whether the downloaded URL is malicious. Invokes the
  // DownloadProtectionService via the delegate.
  // Next state:
  // - STATE_CHECK_VISITED_REFERRER_BEFORE.
  Result DoCheckDownloadUrl();

  // Callback invoked after the delegate has checked the download URL. Sets the
  // danger type of the download to |danger_type|.
  void CheckDownloadUrlDone(download::DownloadDangerType danger_type);

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
  void ScheduleCallbackAndDeleteSelf(download::DownloadInterruptReason result);

  Profile* GetProfile() const;

  // Determine if the download requires confirmation from the user. For regular
  // downloads, this determination is based on the target disposition, auto-open
  // behavior, among other factors. For an interrupted download, this
  // determination will be based on the interrupt reason. It is assumed that
  // download interruptions always occur after the first round of download
  // target determination is complete.
  DownloadConfirmationReason NeedsConfirmation(
      const base::FilePath& filename) const;

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
  bool is_filetype_handled_safely_;
#if defined(OS_ANDROID)
  bool is_checking_dialog_confirmed_path_;
#endif

  download::DownloadItem* download_;
  const bool is_resumption_;
  DownloadPrefs* download_prefs_;
  DownloadTargetDeterminerDelegate* delegate_;
  CompletionCallback completion_callback_;
  base::CancelableTaskTracker history_tracker_;

  base::WeakPtrFactory<DownloadTargetDeterminer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadTargetDeterminer);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_H_
