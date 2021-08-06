// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_types_util.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/prepare_directory_task.h"
#include "components/arc/arc_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_thread.h"

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
constexpr base::TimeDelta kWindowInitializationTimeout =
    base::TimeDelta::FromSeconds(1);

constexpr char kIntentExtraText[] = "android.intent.extra.TEXT";

constexpr base::FilePath::CharType kArcNearbyShareDirname[] =
    FILE_PATH_LITERAL(".NearbyShare");
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
  session_receiver_.set_disconnect_handler(
      base::BindOnce(&NearbyShareSessionImpl::OnSessionDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  aura::Window* const arc_window = GetArcWindow(task_id_);
  if (arc_window) {
    VLOG(1) << "ARC window found";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

NearbyShareSessionImpl::~NearbyShareSessionImpl() {
  env_observation_.Reset();
  arc_window_observation_.Reset();
}

void NearbyShareSessionImpl::OnNearbyShareClosed(
    views::Widget::ClosedReason reason) {
  DCHECK(session_instance_);

  session_instance_->OnNearbyShareViewClosed();
  if (window_initialization_timer_.IsRunning()) {
    window_initialization_timer_.Stop();
  }
  if (reason != views::Widget::ClosedReason::kAcceptButtonClicked) {
    // If share is not continuing after sharesheet closes (e.g. cancel, esc key,
    // lost focus, etc.), we will clean up the current session including files.
    OnCleanupSession();
  }
}

// Overridden from aura::EnvObserver:
void NearbyShareSessionImpl::OnWindowInitialized(aura::Window* const window) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(window);

  if (ash::IsArcWindow(window) && (arc::GetWindowTaskId(window) == task_id_)) {
    env_observation_.Reset();
    arc_window_observation_.Observe(window);
  }
}

// Overridden from aura::WindowObserver
void NearbyShareSessionImpl::OnWindowVisibilityChanged(
    aura::Window* const window,
    bool visible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<int> task_id = arc::GetWindowTaskId(window);
  DCHECK(task_id.has_value());
  DCHECK_GE(task_id.value(), 0);
  if (visible && (base::checked_cast<uint32_t>(task_id.value()) == task_id_)) {
    VLOG(1) << "ARC Window is visible";
    if (window_initialization_timer_.IsRunning()) {
      window_initialization_timer_.Stop();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&NearbyShareSessionImpl::OnArcWindowFound,
                                  weak_ptr_factory_.GetWeakPtr(), window));
  }
}

void NearbyShareSessionImpl::OnArcWindowFound(aura::Window* const arc_window) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window);
  DCHECK(profile_);

  if (share_info_->files.has_value()) {
    // File sharing.
    base::FilePath arc_nearby_share_directory =
        file_manager::util::GetMyFilesFolderForProfile(profile_).Append(
            kArcNearbyShareDirname);

    file_handler_ = base::MakeRefCounted<ShareInfoFileHandler>(
        profile_, share_info_.get(), arc_nearby_share_directory,
        backend_task_runner_);

    VLOG(1) << "Starting PrepareDirectoryTask";
    prepare_directory_task_ = std::make_unique<webshare::PrepareDirectoryTask>(
        arc_nearby_share_directory, file_handler_->GetTotalSizeOfFiles());
    prepare_directory_task_->StartWithCallback(
        base::BindOnce(&NearbyShareSessionImpl::OnPreparedDirectory,
                       weak_ptr_factory_.GetWeakPtr(), arc_window));
  } else {
    // Sharing text.
    ShowNearbyShareBubbleInArcWindow(arc_window);
  }
}

apps::mojom::IntentPtr NearbyShareSessionImpl::ConvertShareIntentInfoToIntent()
    const {
  DCHECK(share_info_);
  DCHECK(share_info_->files);

  std::string text;
  if (share_info_->extras.has_value() &&
      share_info_->extras->contains(kIntentExtraText)) {
    text = share_info_->extras->at(kIntentExtraText);
  }

  // Sharing files & text
  if (share_info_->files.has_value()) {
    const auto share_file_paths = file_handler_->GetFilePaths();
    DCHECK_GT(share_file_paths.size(), 0);
    const auto share_file_mime_types = file_handler_->GetMimeTypes();
    const size_t expected_total_files = file_handler_->GetNumberOfFiles();
    DCHECK_GT(expected_total_files, 0);

    if (share_file_paths.size() != expected_total_files) {
      LOG(ERROR)
          << "Actual number of files streamed does not match expected number: "
          << expected_total_files;
      return nullptr;
    }
    return apps_util::CreateShareIntentFromFiles(profile_, share_file_paths,
                                                 share_file_mime_types, text,
                                                 share_info_->title);
  }

  // Sharing only text
  if (!text.empty()) {
    apps::mojom::IntentPtr share_intent =
        apps_util::CreateShareIntentFromText(text, share_info_->title);
    share_intent->mime_type = share_info_->mime_type;
    return share_intent;
  }
  VLOG(1) << "No Sharing info found";
  return nullptr;
}

void NearbyShareSessionImpl::OnPreparedDirectory(aura::Window* const arc_window,
                                                 base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window);
  DCHECK_GT(file_handler_->GetTotalSizeOfFiles(), 0);

  // TODO(b/191232168): Figure out why PrepareDirectoryTask is flaky. Ignoring
  // the error seem to always work otherwise will sometimes return error.
  PLOG_IF(WARNING, result != base::File::FILE_OK)
      << "Prepare Directory was not successful";

  VLOG(1) << "Preparing files and start streaming from ARC virtual filesystem";
  file_handler_->StartPreparingFiles(
      base::BindOnce(&NearbyShareSessionImpl::ShowNearbyShareBubbleInArcWindow,
                     weak_ptr_factory_.GetWeakPtr(), arc_window),
      base::BindRepeating(&NearbyShareSessionImpl::OnProgressBarUpdate,
                          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareSessionImpl::OnNearbyShareBubbleShown(
    sharesheet::SharesheetResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(session_instance_);

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
    session_instance_->OnNearbyShareViewClosed();
    OnCleanupSession();
  }
}

void NearbyShareSessionImpl::ShowNearbyShareBubbleInArcWindow(
    aura::Window* const arc_window,
    absl::optional<base::File::Error> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc_window);

  // Only applicable if sharing files.
  if (result.has_value() && result.value() != base::File::FILE_OK) {
    LOG(ERROR) << "Failed to complete file streaming with error: "
               << base::File::ErrorToString(result.value());
    OnCleanupSession();
    return;
  }

  VLOG(1) << "Getting Sharesheet service";
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  if (!sharesheet_service) {
    LOG(ERROR) << "Cannot find sharesheet service.";
    OnCleanupSession();
    return;
  }

  apps::mojom::IntentPtr intent = ConvertShareIntentInfoToIntent();
  if (!intent) {
    LOG(ERROR) << "No share info found.";
    OnCleanupSession();
    return;
  }

  VLOG(1) << "Calling ShowNearbyShareBubbleForArc";
  sharesheet_service->ShowNearbyShareBubbleForArc(
      arc_window, std::move(intent),
      sharesheet::SharesheetMetrics::LaunchSource::kArcNearbyShare,
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareBubbleShown,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&NearbyShareSessionImpl::OnNearbyShareClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&NearbyShareSessionImpl::OnCleanupSession,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyShareSessionImpl::OnTimerFired() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(session_instance_);

  // TODO(phshah): Handle error case and add UMA metric.
  LOG(ERROR) << "ARC window didn't get initialized within "
             << kWindowInitializationTimeout.InSeconds() << " second(s)";

  session_instance_->OnNearbyShareViewClosed();
  OnCleanupSession();
}

void NearbyShareSessionImpl::OnProgressBarUpdate(double value) {
  // TODO(b/191705289): Add UI integration with views::ProgressBar.
  VLOG(1) << "Called OnProgressBarUpdate with value: " << value;
}

void NearbyShareSessionImpl::OnSessionDisconnected() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  aura::Window* const arc_window = GetArcWindow(task_id_);
  if (!arc_window) {
    LOG(ERROR) << "Unable to close sharesheet bubble. No ARC window found for "
               << "task ID: " << task_id_;
    OnCleanupSession();
    return;
  }

  VLOG(1) << "Getting Sharesheet service";
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  if (!sharesheet_service) {
    LOG(ERROR) << "Unable to close sharesheet bubble. Cannot find sharesheet "
                  "service.";
    OnCleanupSession();
    return;
  }
  sharesheet_service->CloseBubble(arc_window,
                                  sharesheet::SharesheetResult::kCancel);
}

void NearbyShareSessionImpl::OnCleanupSession() {
  DCHECK(session_finished_callback_);

  VLOG(1) << "Called OnCleanupSession";
  // PrepareDirectoryTask must first relinquish ownership of |share_path|.
  prepare_directory_task_.reset();
  if (file_handler_) {
    const base::FilePath share_path = file_handler_->GetShareDirectory();
    // Delete the temp directories created during the current session using
    // the |backend_task_runner_| passed to |file_handler|.
    file_handler_.reset();
    if (!share_path.empty() && base::PathExists(share_path)) {
      // Delete any other lingering directories / files and the top level share
      // directory on the same |backend_task_runner_|.
      backend_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(base::GetDeletePathRecursivelyCallback(), share_path));
    }
  }
  // Delete the current session object by using the task ID associated with it.
  std::move(session_finished_callback_).Run(task_id_);
}

}  // namespace arc
