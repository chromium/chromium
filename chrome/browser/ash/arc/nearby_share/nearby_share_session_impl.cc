// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_uma.h"
#include "chrome/browser/ash/arc/nearby_share/ui/error_dialog_view.h"
#include "chrome/browser/ash/arc/nearby_share/ui/low_disk_space_dialog_view.h"
#include "chrome/browser/ash/arc/nearby_share/ui/nearby_share_overlay_view.h"
#include "chrome/browser/ash/arc/nearby_share/ui/progress_bar_dialog_view.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {
// Maximum time to wait for the ARC window to be initialized.
// ARC Wayland messages and Mojo messages are sent across the same pipe. The
// order in which the messages are sent is not deterministic. If the ARC
// activity starts Nearby Share before the wayland message for the new has been
// processed, the corresponding aura::Window for a given ARC activity task ID
// will not be found. To get around this, NearbyShareSessionImpl will wait a
// little while for the Wayland message to be processed and the window to be
// initialized.
constexpr base::TimeDelta kWindowInitializationTimeout = base::Seconds(1);

constexpr base::TimeDelta kProgressBarUpdateInterval = base::Milliseconds(1500);

constexpr uint64_t kShowProgressBarMinSizeInBytes = 12000000;  // 12MB.

constexpr char kIntentExtraText[] = "android.intent.extra.TEXT";

constexpr base::FilePath::CharType kArcNearbyShareDirname[] =
    FILE_PATH_LITERAL(".NearbyShare");

void DeletePathAndFiles(const base::FilePath& file_path) {
  DVLOG(1) << __func__;
  if (!file_path.empty() && base::PathExists(file_path)) {
    base::DeletePathRecursively(file_path);
  }
}

void DoDeleteShareCacheFilePaths(const base::FilePath& profile_path,
                                 const base::FilePath& user_cache_file_path) {
  // Up until M99, shared files were stored in <user_cache_dir>/.NearbyShare.
  // We should remove this obsolete directory path if it is still present.
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_base_path);
  DeletePathAndFiles(cache_base_path.Append(kArcNearbyShareDirname));

  // Delete the current user cache file path.
  DeletePathAndFiles(user_cache_file_path);
}

// Calculate the amount of disk space, in bytes, needed in |share_dir| to
// stream |total_file_size| bytes from Android to the Chrome OS file system.
static int64_t CalculateRequiredSpace(const base::FilePath& share_dir,
                                      const uint64_t total_file_size) {
  DVLOG(1) << __func__;
  int64_t free_disk_space = base::SysInfo::AmountOfFreeDiskSpace(share_dir);
  VLOG(1) << "Free disk space: " << free_disk_space;
  int64_t shared_files_size =
      static_cast<int64_t>(cryptohome::kMinFreeSpaceInBytes + total_file_size);
  VLOG(1) << "Shared file size: " << shared_files_size;
  return shared_files_size - free_disk_space;
}

base::FilePath GetUserCacheFilePath(Profile* const profile) {
  DCHECK(profile);
  base::FilePath file_path = file_manager::util::GetShareCacheFilePath(profile);
  return file_path.Append(kArcNearbyShareDirname);
}

void DeleteSharedFiles(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(&DeletePathAndFiles, file_path));
}

bool IsValidArcWindow(aura::Window* const window, uint32_t task_id) {
  if (!ash::IsArcWindow(window)) {
    return false;
  }

  std::optional<int> maybe_task_id = arc::GetWindowTaskId(window);
  if (!maybe_task_id.has_value() || maybe_task_id.value() < 0 ||
      static_cast<uint32_t>(maybe_task_id.value()) != task_id) {
    return false;
  }

  return true;
}

}  // namespace

NearbyShareSessionImpl::NearbyShareSessionImpl(
    Profile* profile,
    uint32_t task_id,
    mojom::ShareIntentInfoPtr share_info,
    mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
    mojo::PendingReceiver<mojom::NearbyShareSessionHost> session_receiver,
    SessionFinishedCallback session_finished_callback)
    : task_id_(task_id),
      session_instance_(std::move(session_instance)),
      session_receiver_(this, std::move(session_receiver)),
      share_info_(std::move(share_info)),
      profile_(profile),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // Should be USER_VISIBLE because we are downloading files requested
          // by the user and updating the UI on progress of transfers.
          // IO operations for temp files / directories cleanup should be
          // completed before shutdown.
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      session_finished_callback_(std::move(session_finished_callback)) {
  session_receiver_.set_disconnect_handler(base::BindOnce(
      &NearbyShareSessionImpl::CleanupSession, weak_ptr_factory_.GetWeakPtr(),
      /*should_cleanup_files=*/true));
  aura::Window* const arc_window = GetArcWindow(task_id_);
  if (arc_window) {
    VLOG(1) << "ARC window found.";
    UpdateNearbyShareWindowFound(true);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&NearbyShareSessionImpl::OnArcWindowFound,
                                  weak_ptr_factory_.GetWeakPtr(), arc_window));
  } else {
    VLOG(1) << "No ARC window found for task ID: " << task_id_;
    env_observation_.Observe(aura::Env::GetInstance());
    window_initialization_timer_.Start(FROM_HERE, kWindowInitializationTimeout,
                                       this,
                                       &NearbyShareSessionImpl::OnTimerFired);
  }
}

NearbyShareSessionImpl::~NearbyShareSessionImpl() = default;

// static
void NearbyShareSessionImpl::DeleteShareCacheFilePaths(Profile* const profile) {
  DCHECK(profile);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DoDeleteShareCacheFilePaths, profile->GetPath(),
                     GetUserCacheFilePath(profile)));
}

void NearbyShareSessionImpl::OnNearbyShareClosed(
    views::Widget::ClosedReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << __func__;
  // If share is not continuing after sharesheet closes (e.g. cancel, esc key,
  // lost focus, etc.), we will clean up the current session including files.
  // Otherwise cleanup session object and wait for Nearby Share to cleanup cache
  // files when they are no longer needed for transfer.
  bool should_cleanup_files =
      reason != views::Widget::ClosedReason::kAcceptButtonClicked;
  CleanupSession(should_cleanup_files);
}

// Overridden from aura::EnvObserver:
void NearbyShareSessionImpl::OnWindowInitialized(aura::Window* const window) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(window);

  DVLOG(1) << __func__;
  if (!IsValidArcWindow(window, task_id_)) {
    return;
  }

  env_observation_.Reset();
  arc_window_observation_.Observe(window);
}

// Overridden from aura::WindowObserver
void NearbyShareSessionImpl::OnWindowVisibilityChanged(
    aura::Window* const window,
    bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << __func__;
  if (!IsValidArcWindow(window, task_id_) || !visible) {
    return;
  }

  VLOG(1) << "ARC Window is visible.";
  if (window_initialization_timer_.IsRunning()) {
    window_initialization_timer_.Stop();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NearbyShareSessionImpl::OnArcWindowFound,
                                weak_ptr_factory_.GetWeakPtr(), window));
}

void NearbyShareSessionImpl::OnArcWindowFound(aura::Window* const arc_window) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window);
  DCHECK(profile_);

  DVLOG(1) << __func__;
  arc_window_ = arc_window;
  if (!share_info_->files.has_value()) {
    // When only sharing text, we don't need to prepare files, and can show the
    // bubble immediately.
    ShowNearbyShareBubbleInArcWindow();
    return;
  }

  // Sharing file(s) through temporary copy.
  const base::FilePath arc_nearby_share_directory =
      GetUserCacheFilePath(profile_);

  file_handler_ = base::MakeRefCounted<ShareInfoFileHandler>(
      profile_, share_info_.get(), arc_nearby_share_directory,
      backend_task_runner_);

  prepare_directory_task_ = std::make_unique<webshare::PrepareDirectoryTask>(
      arc_nearby_share_directory, file_handler_->GetTotalSizeOfFiles());
  prepare_directory_task_->StartWithCallback(
      base::BindOnce(&NearbyShareSessionImpl::OnPreparedDirectory,
                     weak_ptr_factory_.GetWeakPtr()));
}

apps::IntentPtr NearbyShareSessionImpl::ConvertShareIntentInfoToIntent() {
  DCHECK(share_info_);

  DVLOG(1) << __func__;
  // Sharing files
  if (share_info_->files.has_value()) {
    const auto share_file_paths = file_handler_->GetFilePaths();
    DCHECK_GT(share_file_paths.size(), 0u);
    const auto share_file_mime_types = file_handler_->GetMimeTypes();
    const size_t expected_total_files = file_handler_->GetNumberOfFiles();
    DCHECK_GT(expected_total_files, 0u);

    if (share_file_paths.size() != expected_total_files) {
      LOG(ERROR)
          << "Actual number of files streamed does not match expected number: "
          << expected_total_files;
      return nullptr;
    }
    return apps_util::CreateShareIntentFromFiles(
        profile_, share_file_paths, share_file_mime_types, std::string(),
        share_info_->title);
  }

  // Sharing text
  if (share_info_->extras.has_value() &&
      share_info_->extras->contains(kIntentExtraText)) {
    apps::IntentPtr share_intent = apps_util::MakeShareIntent(
        share_info_->extras->at(kIntentExtraText), share_info_->title);
    share_intent->mime_type = share_info_->mime_type;
    return share_intent;
  }
  return nullptr;
}

void NearbyShareSessionImpl::OnPreparedDirectory(base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window_);
  DCHECK_GT(file_handler_->GetTotalSizeOfFiles(), 0u);

  DVLOG(1) << __func__;
  if (result == base::File::FILE_ERROR_NO_SPACE) {
    LOG(ERROR) << "Not enough disk space to proceed with sharing.";
    // Calculate required space and then show the LowDiskSpace Dialog
    backend_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CalculateRequiredSpace,
                       file_handler_->GetShareDirectory(),
                       file_handler_->GetTotalSizeOfFiles()),
        base::BindOnce(&NearbyShareSessionImpl::OnShowLowDiskSpaceDialog,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // PrepareDirectoryTask can sometimes be flaky but the error does not affect
  // functionality. Log a warning when this happens and continue.
  PLOG_IF(WARNING, result != base::File::FILE_OK)
      << "Prepare Directory was not successful";

  file_handler_->StartPreparingFiles(
      /*started_callback=*/base::BindOnce(
          &NearbyShareSessionImpl::OnFileStreamingStarted,
          weak_ptr_factory_.GetWeakPtr()),
      /*completed_callback=*/
      base::BindOnce(&NearbyShareSessionImpl::ShowNearbyShareBubbleInArcWindow,
                     weak_ptr_factory_.GetWeakPtr()),
      /*update_callback=*/
      base::BindRepeating(&NearbyShareSessionImpl::OnProgressBarUpdate,
                          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareSessionImpl::OnNearbyShareBubbleShown(
    sharesheet::SharesheetResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (VLOG_IS_ON(1)) {
    switch (result) {
      case sharesheet::SharesheetResult::kSuccess:
        VLOG(1) << "OnNearbyShareBubbleShown: SUCCESS";
        break;
      case sharesheet::SharesheetResult::kCancel:
        VLOG(1) << "OnNearbyShareBubbleShown: CANCEL";
        break;
      case sharesheet::SharesheetResult::kErrorAlreadyOpen:
        VLOG(1) << "OnNearbyShareBubbleShown: ALREADY OPEN";
        break;
      default:
        VLOG(1) << "OnNearbyShareBubbleShown: UNKNOWN";
    }
  }
  if (result != sharesheet::SharesheetResult::kSuccess) {
    ShowErrorDialog();
  }
}

void NearbyShareSessionImpl::OnFileStreamingStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window_);
  DCHECK(file_handler_);

  DVLOG(1) << __func__;
  // Only show the progress bar if total files size is greater than
  // |kShowProgressBarMinSizeInBytes|, otherwise minimize unnecessary
  // UI step as the file streaming time should be < 1 second.
  if (file_handler_->GetTotalSizeOfFiles() > kShowProgressBarMinSizeInBytes) {
    const bool is_multiple_files = file_handler_->GetNumberOfFiles() > 1;
    progress_bar_view_ =
        std::make_unique<ProgressBarDialogView>(is_multiple_files);
    ProgressBarDialogView::Show(arc_window_, progress_bar_view_.get());

    // Keep updating the progress bar if the interval timer elapsed to update
    // the user on the file streaming progress.
    progress_bar_update_timer_.Start(
        FROM_HERE, kProgressBarUpdateInterval, this,
        &NearbyShareSessionImpl::OnProgressBarIntervalElapsed);
  }
}

void NearbyShareSessionImpl::ShowNearbyShareBubbleInArcWindow(
    std::optional<base::File::Error> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window_);

  DVLOG(1) << __func__;

  // If the progress bar is visible at this point, stop the update interval
  // timer so that we can show the Nearby Share bubble.
  if (progress_bar_update_timer_.IsRunning()) {
    progress_bar_update_timer_.Stop();
  }

  // Close any overlay and respective child views that may still be shown.
  progress_bar_view_.reset();
  NearbyShareOverlayView::CloseOverlayOn(arc_window_);

  // Only applicable if sharing files.
  if (result.has_value() && result.value() != base::File::FILE_OK) {
    LOG(ERROR) << "Failed to complete file streaming with error: "
               << base::File::ErrorToString(result.value());
    UpdateNearbyShareFileStreamError(result.value());
    ShowErrorDialog();
    return;
  }

  auto intent = ConvertShareIntentInfoToIntent();

  if (!intent) {
    LOG(ERROR) << "No share info found.";
    ShowErrorDialog();
    return;
  }

  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  if (!sharesheet_service) {
    LOG(ERROR) << "Cannot find sharesheet service.";
    ShowErrorDialog();
    return;
  }

  base::FilePath share_path;
  if (file_handler_) {
    share_path = file_handler_->GetShareDirectory();
  }

  sharesheet::DeliveredCallback delivered_callback =
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareBubbleShown,
                     weak_ptr_factory_.GetWeakPtr());
  sharesheet::CloseCallback close_callback =
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareClosed,
                     weak_ptr_factory_.GetWeakPtr());
  sharesheet::ActionCleanupCallback cleanup_callback =
      base::BindOnce(&DeleteSharedFiles, share_path);

  if (test_sharesheet_callback_) {
    test_sharesheet_callback_.Run(arc_window_.get(), std::move(intent),
                                  sharesheet::LaunchSource::kArcNearbyShare,
                                  std::move(delivered_callback),
                                  std::move(close_callback),
                                  std::move(cleanup_callback));
    return;
  }

  sharesheet_service->ShowNearbyShareBubbleForArc(
      arc_window_, std::move(intent), sharesheet::LaunchSource::kArcNearbyShare,
      std::move(delivered_callback), std::move(close_callback),
      std::move(cleanup_callback));
}

void NearbyShareSessionImpl::OnTimerFired() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(b/191232397): Handle error case.
  LOG(ERROR) << "ARC window didn't get initialized within "
             << kWindowInitializationTimeout.InSeconds() << " second(s).";
  UpdateNearbyShareWindowFound(false);
  CleanupSession(/*should_cleanup_files=*/true);
}

void NearbyShareSessionImpl::OnProgressBarIntervalElapsed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << __func__;
  if (progress_bar_view_) {
    progress_bar_view_->UpdateInterpolatedProgressBarValue();
  }
}

void NearbyShareSessionImpl::OnProgressBarUpdate(double value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "OnProgressBarUpdate with value: " << value;
  if (progress_bar_view_) {
    // Only show value if there is forward progress.
    if (value > progress_bar_view_->GetProgressBarValue()) {
      progress_bar_view_->UpdateProgressBarValue(value);
    }
  }
}

void NearbyShareSessionImpl::CleanupSession(bool should_cleanup_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << __func__;

  // PrepareDirectoryTask must first relinquish ownership of |share_path|.
  prepare_directory_task_.reset();
  if (file_handler_) {
    base::FilePath share_path = file_handler_->GetShareDirectory();
    // Delete any file descriptor handles for files that were created during
    // the session and owned by |file_handler_|. Even if handles are released,
    // the physical files remain until DeletePathAndFiles is called.
    file_handler_.reset();
    if (should_cleanup_files) {
      VLOG(1) << "Deleting session files including base path: " << share_path;
      // Delete any files and the top level share directory with the same
      // |backend_task_runner_| that is used for all file IO operations.
      // Make sure |session_finished_callback_| is not run until the
      // |backend_task_runner_| is no longer in use.
      backend_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&DeletePathAndFiles, share_path),
          base::BindOnce(&NearbyShareSessionImpl::FinishSession,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
  FinishSession();
}

void NearbyShareSessionImpl::ShowErrorDialog() {
  DCHECK(arc_window_);

  DVLOG(1) << __func__;
  ErrorDialogView::Show(arc_window_,
                        base::BindOnce(&NearbyShareSessionImpl::CleanupSession,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       /*should_cleanup_files=*/true));
}

void NearbyShareSessionImpl::FinishSession() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(session_instance_);

  DVLOG(1) << __func__;
  // Stop timers and destroy any lingering UI surfaces or observers.
  arc_window_observation_.Reset();
  env_observation_.Reset();
  if (window_initialization_timer_.IsRunning()) {
    window_initialization_timer_.Stop();
  }
  if (progress_bar_update_timer_.IsRunning()) {
    progress_bar_update_timer_.Stop();
  }
  progress_bar_view_.reset();
  aura::Window* const arc_window = GetArcWindow(task_id_);
  if (arc_window) {
    NearbyShareOverlayView::CloseOverlayOn(arc_window);
  }

  // Cleanup the session on Android side.
  session_instance_->OnNearbyShareViewClosed();
  // Delete the session object by task ID from the ArcNearbyShareBridge map.
  if (session_finished_callback_) {
    VLOG(1) << "Deleting session with task ID: " << task_id_;
    std::move(session_finished_callback_).Run(task_id_);
  }
}

void NearbyShareSessionImpl::OnShowLowDiskSpaceDialog(
    int64_t required_disk_space) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GT(required_disk_space, 0);

  DVLOG(1) << "OnCalculateRequiredSpace required_disk_space: "
           << required_disk_space;
  LowDiskSpaceDialogView::Show(
      arc_window_, file_handler_->GetNumberOfFiles(), required_disk_space,
      base::BindOnce(&NearbyShareSessionImpl::OnLowStorageDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareSessionImpl::OnLowStorageDialogClosed(
    bool should_open_storage_settings) {
  if (should_open_storage_settings) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile_, chromeos::settings::mojom::kStorageSubpagePath);
  }
  CleanupSession(/*should_cleanup_files=*/true);
}

}  // namespace arc
