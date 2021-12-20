// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/download/bubble/download_display_controller.h"

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/download_item_model.h"

namespace {
constexpr int kToolbarIconVisbilityHours = 24;
}

DownloadDisplayController::DownloadDisplayController(
    DownloadDisplay* display,
    content::DownloadManager* download_manager)
    : display_(display),
      download_manager_(download_manager),
      download_notifier_(download_manager, this) {}

DownloadDisplayController::~DownloadDisplayController() = default;

void DownloadDisplayController::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  UpdateToolbarButtonState();
}

void DownloadDisplayController::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DownloadItemModel item_model(item);

  if (item_model.IsDone()) {
    ScheduleToolbarDisappearance(base::Hours(kToolbarIconVisbilityHours));
  }
  UpdateToolbarButtonState();
}

void DownloadDisplayController::ShowToolbarButton() {
  if (!display_->IsShowing()) {
    display_->Enable();
    display_->Show();
  }
}

void DownloadDisplayController::HideToolbarButton() {
  if (display_->IsShowing()) {
    display_->Hide();
  }
}

void DownloadDisplayController::UpdateToolbarButtonState() {
  download::DownloadIconState icon_state;

  if (download_manager_->InProgressCount() > 0) {
    ShowToolbarButton();
    icon_state = download::DownloadIconState::kProgress;
  } else {
    icon_state = download::DownloadIconState::kComplete;
  }

  display_->UpdateDownloadIcon(icon_state);
}

void DownloadDisplayController::ScheduleToolbarDisappearance(
    base::TimeDelta interval) {
  icon_disappearance_timer_.Stop();
  icon_disappearance_timer_.Start(
      FROM_HERE, interval, this, &DownloadDisplayController::HideToolbarButton);
}

DownloadDisplayController::ProgressInfo
DownloadDisplayController::GetProgress() {
  DownloadDisplayController::ProgressInfo progress_info;

  int64_t received_bytes = 0;
  int64_t total_bytes = 0;

  content::DownloadManager::DownloadVector items;
  download_manager_->GetAllDownloads(&items);
  for (auto* item : items) {
    if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
      ++progress_info.download_count;
      if (item->GetTotalBytes() <= 0) {
        // There may or may not be more data coming down this pipe.
        progress_info.progress_certain = false;
      } else {
        received_bytes += item->GetReceivedBytes();
        total_bytes += item->GetTotalBytes();
      }
    }
  }

  if (total_bytes > 0) {
    progress_info.progress_percentage =
        base::ClampFloor(received_bytes * 100.0 / total_bytes);
  }

  return progress_info;
}
