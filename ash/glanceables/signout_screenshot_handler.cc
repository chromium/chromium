// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/signout_screenshot_handler.h"

#include "ash/glanceables/glanceables_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/snapshot/snapshot.h"

namespace ash {
namespace {

// Writes `png_data` to disk at `file_path`.
void WriteScreenshotOnBlockingPool(
    const base::FilePath& file_path,
    scoped_refptr<base::RefCountedMemory> png_data) {
  auto raw_data = base::make_span(png_data->data(), png_data->size());
  if (!base::WriteFile(file_path, raw_data))
    LOG(ERROR) << "Failed to write screenshot " << file_path.MaybeAsASCII();
}

// Deletes the file at `file_path`.
void DeleteScreenshotOnBlockingPool(const base::FilePath& file_path) {
  base::DeleteFile(file_path);
}

scoped_refptr<base::RefCountedMemory> EncodeImageAsPngOnThreadPool(
    const gfx::Image& image) {
  DCHECK(!image.IsEmpty());
  return image.As1xPNGBytes();
}

}  // namespace

SignoutScreenshotHandler::SignoutScreenshotHandler() = default;

SignoutScreenshotHandler::~SignoutScreenshotHandler() = default;

void SignoutScreenshotHandler::TakeScreenshot(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);
  start_time_ = base::TimeTicks::Now();

  // TODO(crbug.com/1353119): Support multiple displays. For now, use the most
  // recently active display.
  aura::Window* root = Shell::GetRootWindowForNewWindows();
  DCHECK(root);
  // The screenshot should only contain windows, not UI like the shelf. Take a
  // screenshot of the active desk container.
  aura::Window* active_desk = desks_util::GetActiveDeskContainerForRoot(root);
  DCHECK(active_desk);
  if (active_desk->children().empty()) {
    // If there are no windows in the desk container, taking the screenshot will
    // fail. Delete any existing screenshot so we know on startup that there are
    // no windows to preview.
    DeleteScreenshot();
    return;
  }
  gfx::Size source_size = active_desk->bounds().size();
  // Capture the screenshot at a smaller size than the desk. This speeds up PNG
  // encoding and writing to disk.
  screenshot_size_ =
      gfx::Size(source_size.width() / 2, source_size.height() / 2);
  // Snapshot scaling uses skia::ImageOperations::RESIZE_GOOD which should be
  // fast. See SnapshotAsync::ScaleCopyOutputResult().
  ui::GrabWindowSnapshotAndScaleAsync(
      active_desk,
      /*source_rect=*/gfx::Rect(gfx::Point(), source_size), screenshot_size_,
      base::BindOnce(&SignoutScreenshotHandler::OnScreenshotTaken,
                     weak_factory_.GetWeakPtr()));
}

void SignoutScreenshotHandler::OnScreenshotTaken(gfx::Image image) {
  if (image.IsEmpty()) {
    // If the screenshot failed, delete any existing screenshot so we don't show
    // a stale image on startup.
    DeleteScreenshot();
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&EncodeImageAsPngOnThreadPool, std::move(image)),
      base::BindOnce(&SignoutScreenshotHandler::SaveScreenshot,
                     weak_factory_.GetWeakPtr()));
}

void SignoutScreenshotHandler::SaveScreenshot(
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (!png_data) {
    // If PNG encoding failed failed, delete any existing screenshot so we don't
    // show a stale image on startup.
    DeleteScreenshot();
    return;
  }
  base::FilePath file_path = GetScreenshotPath();
  // Use priority USER_BLOCKING since the user is waiting for logout/shutdown.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteScreenshotOnBlockingPool, file_path, png_data),
      base::BindOnce(&SignoutScreenshotHandler::OnScreenshotSaved,
                     weak_factory_.GetWeakPtr()));
}

void SignoutScreenshotHandler::OnScreenshotSaved() {
  // Record the screenshot duration to a pref, which will be saved as part of
  // shutdown. The UMA metric will be recorded on the next startup.
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  glanceables_util::SaveSignoutScreenshotDuration(Shell::Get()->local_state(),
                                                  duration);
  std::move(done_callback_).Run();
}

void SignoutScreenshotHandler::DeleteScreenshot() {
  base::FilePath file_path = GetScreenshotPath();
  // Use priority USER_BLOCKING since the user is waiting for logout/shutdown.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DeleteScreenshotOnBlockingPool, file_path),
      base::BindOnce(&SignoutScreenshotHandler::OnScreenshotDeleted,
                     weak_factory_.GetWeakPtr()));
}

void SignoutScreenshotHandler::OnScreenshotDeleted() {
  std::move(done_callback_).Run();
}

base::FilePath SignoutScreenshotHandler::GetScreenshotPath() const {
  if (!screenshot_path_for_test_.empty())
    return screenshot_path_for_test_;
  return glanceables_util::GetSignoutScreenshotPath();
}

}  // namespace ash
