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
#include "chrome/browser/android/compose_bitmaps_helper.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

namespace {

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

  aura::Window::Windows all_windows;

  // In unit tests, shell is not created.
  if (ash::Shell::HasInstance()) {
    all_windows = ash::Shell::GetAllRootWindows();
  }

  if (all_windows.size() == 0) {
    std::move(callback).Run(false);
    return;
  }

  auto barrier_callback =
      base::BarrierCallback<scoped_refptr<base::RefCountedMemory>>(
          all_windows.size(),
          base::BindOnce(&OsFeedbackScreenshotManager::OnAllScreenshotsTaken,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

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
    ScreenshotCallback callback,
    std::vector<scoped_refptr<base::RefCountedMemory>> all_data) {
  std::vector<scoped_refptr<base::RefCountedMemory>> screenshot_data_set;
  for (scoped_refptr<base::RefCountedMemory> data : all_data) {
    if (data && data.get()) {
      screenshot_data_set.push_back(data);
    }
  }
  if (screenshot_data_set.size() > 0) {
    screenshot_png_data_ = GetCombinedBitmap(screenshot_data_set);
    std::move(callback).Run(true);
  } else {
    LOG(ERROR) << "failed to take screenshot.";
    std::move(callback).Run(false);
  }
}

void OsFeedbackScreenshotManager::DeleteScreenshotData() {
  // TODO(xiangdongkong): See whether we can delete this.
  screenshot_png_data_ = nullptr;
}

}  // namespace ash
