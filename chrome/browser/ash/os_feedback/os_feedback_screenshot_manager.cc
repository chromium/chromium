// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"

#include <utility>

#include "ash/shell.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

namespace {

bool IsScreenshotsDisabledByPolicy() {
  if (!ash::Shell::HasInstance()) {
    return false;
  }
  PrefService* local_state = ash::Shell::Get()->local_state();
  return local_state &&
         local_state->GetBoolean(policy::policy_prefs::kDisableScreenshots);
}

// Helper method to collect all png_data before calling OnAllScreenshotsTaken
// function to handle them.
void OnOneScreenshotTaken(
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>
        barrier_callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  std::move(barrier_callback).Run(png_data);
}

// Helper method to combine all the screenshots in the screenshot_data_set in
// horizontal direction.
scoped_refptr<base::RefCountedMemory> GetCombinedBitmap(
    const std::vector<scoped_refptr<base::RefCountedMemory>>&
        screenshot_data_set) {
  // If we only have one displays, skip combining.
  if (screenshot_data_set.size() == 1) {
    return screenshot_data_set[0];
  }

  int32_t total_width = 0;
  int32_t total_height = 0;

  std::vector<SkBitmap> bitmaps;
  for (const auto& data : screenshot_data_set) {
    auto image = gfx::Image::CreateFrom1xPNGBytes(data);
    auto bitmap = std::move(*image.ToSkBitmap());
    total_width += bitmap.width();
    total_height = std::max(total_height, bitmap.height());
    bitmaps.push_back(bitmap);
  }

  SkImageInfo image_info = bitmaps[0]
                               .info()
                               .makeWH(total_width, total_height)
                               .makeAlphaType(kPremul_SkAlphaType);

  SkBitmap combined_bitmap;
  combined_bitmap.setInfo(image_info);
  combined_bitmap.allocPixels();

  int32_t next_start_pixel = 0;
  for (auto& bitmap : bitmaps) {
    combined_bitmap.writePixels(bitmap.pixmap(), next_start_pixel, 0);
    next_start_pixel += bitmap.dimensions().width();
  }
  return gfx::Image::CreateFrom1xBitmap(std::move(combined_bitmap))
      .As1xPNGBytes();
}

}  // namespace

OsFeedbackScreenshotManager::OsFeedbackScreenshotManager() = default;
OsFeedbackScreenshotManager::~OsFeedbackScreenshotManager() = default;

// Static.
OsFeedbackScreenshotManager* OsFeedbackScreenshotManager::GetInstance() {
  return base::Singleton<OsFeedbackScreenshotManager>::get();
}

OsFeedbackScreenshotManager* OsFeedbackScreenshotManager::GetIfExists() {
  return base::Singleton<OsFeedbackScreenshotManager>::GetIfExists();
}

void OsFeedbackScreenshotManager::TakeScreenshot(ScreenshotCallback callback) {
  // Skip taking if one has been taken already. Although the feedback tool only
  // allows one instance, the user could request it multiple times while one
  // instance is still running.
  if (screenshot_png_data_ != nullptr) {
    std::move(callback).Run(false);
    return;
  }

  // Coalesce concurrent requests while an async DLP check or snapshot capture
  // is already in flight so we neither spawn duplicate DLP warning dialogs nor
  // open the feedback UI before capture completes.
  if (!pending_callbacks_.empty()) {
    pending_callbacks_.push_back(std::move(callback));
    return;
  }

  // In unit tests, shell is not created.
  if (!ash::Shell::HasInstance() || ash::Shell::GetAllRootWindows().empty()) {
    std::move(callback).Run(false);
    return;
  }

  if (IsScreenshotsDisabledByPolicy()) {
    std::move(callback).Run(false);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  // Avoid instantiating DlpContentManagerAsh when no DlpContentObserver
  // instance exists (e.g., on the login screen / OOBE), since lazy
  // initialization before primary profile login permanently leaves its
  // reporting manager null. Without an active instance, no confidential
  // content can be registered.
  if (!policy::DlpContentObserver::HasInstance()) {
    OnDlpRestrictionChecked(/*proceed=*/true);
    return;
  }

  policy::DlpContentManagerAsh::Get()->CheckScreenshotRestriction(
      ScreenshotArea::CreateForAllRootWindows(),
      base::BindOnce(&OsFeedbackScreenshotManager::OnDlpRestrictionChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OsFeedbackScreenshotManager::OnDlpRestrictionChecked(bool proceed) {
  if (!proceed || screenshot_png_data_ != nullptr) {
    FinishTakeScreenshot(false);
    return;
  }

  // Re-check policy because CheckScreenshotRestriction() can be async when a
  // DLP warning dialog is shown, during which policy may change.
  if (IsScreenshotsDisabledByPolicy()) {
    FinishTakeScreenshot(false);
    return;
  }

  aura::Window::Windows all_windows;
  if (ash::Shell::HasInstance()) {
    all_windows = ash::Shell::GetAllRootWindows();
  }

  if (all_windows.empty()) {
    FinishTakeScreenshot(false);
    return;
  }

  if (policy::DlpContentObserver::HasInstance()) {
    auto* dlp_manager = policy::DlpContentManagerAsh::Get();
    // Re-check for blocking restrictions that may have appeared while an async
    // DLP warning dialog was open; OnImageCapture() CHECKs that the captured
    // area is not blocked.
    if (dlp_manager->GetOnScreenPresentRestrictions().GetRestrictionLevel(
            policy::DlpContentRestriction::kScreenshot) ==
        policy::DlpRulesManager::Level::kBlock) {
      FinishTakeScreenshot(false);
      return;
    }
    dlp_manager->OnImageCapture(ScreenshotArea::CreateForAllRootWindows());
  }

  auto barrier_callback =
      base::BarrierCallback<scoped_refptr<base::RefCountedMemory>>(
          all_windows.size(),
          base::BindOnce(&OsFeedbackScreenshotManager::OnAllScreenshotsTaken,
                         weak_ptr_factory_.GetWeakPtr()));

  for (aura::Window* root_window : all_windows) {
    gfx::Rect rect = root_window->bounds();
    ui::GrabWindowSnapshotAsPNG(
        root_window, rect,
        base::BindOnce(OnOneScreenshotTaken, barrier_callback));
  }
}

scoped_refptr<base::RefCountedMemory>
OsFeedbackScreenshotManager::GetScreenshotData() {
  return screenshot_png_data_;
}

void OsFeedbackScreenshotManager::OnAllScreenshotsTaken(
    std::vector<scoped_refptr<base::RefCountedMemory>> all_data) {
  std::vector<scoped_refptr<base::RefCountedMemory>> screenshot_data_set;
  for (scoped_refptr<base::RefCountedMemory> data : all_data) {
    if (data && data.get()) {
      screenshot_data_set.push_back(data);
    }
  }
  if (screenshot_data_set.size() > 0) {
    screenshot_png_data_ = GetCombinedBitmap(screenshot_data_set);
    FinishTakeScreenshot(true);
  } else {
    LOG(ERROR) << "failed to take screenshot.";
    FinishTakeScreenshot(false);
  }
}

void OsFeedbackScreenshotManager::FinishTakeScreenshot(bool status) {
  std::vector<ScreenshotCallback> callbacks;
  callbacks.swap(pending_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(status);
  }
}

void OsFeedbackScreenshotManager::DeleteScreenshotData() {
  // TODO(xiangdongkong): See whether we can delete this.
  screenshot_png_data_ = nullptr;
}

}  // namespace ash
