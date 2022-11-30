// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"

#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "ui/aura/window.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

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

  // In unit tests, shell is not created.
  aura::Window* primary_window =
      ash::Shell::HasInstance() ? ash::Shell::GetPrimaryRootWindow() : nullptr;
  if (primary_window == nullptr) {
    std::move(callback).Run(false);
    return;
  }
  gfx::Rect rect = primary_window->bounds();
  ui::GrabWindowSnapshotAsyncPNG(
      primary_window, rect,
      base::BindOnce(&OsFeedbackScreenshotManager::OnScreenshotTaken,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

scoped_refptr<base::RefCountedMemory>
OsFeedbackScreenshotManager::GetScreenshotData() {
  return screenshot_png_data_;
}

void OsFeedbackScreenshotManager::OnScreenshotTaken(
    ScreenshotCallback callback,
    scoped_refptr<base::RefCountedMemory> data) {
  if (data && data.get()) {
    screenshot_png_data_ = std::move(data);
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
