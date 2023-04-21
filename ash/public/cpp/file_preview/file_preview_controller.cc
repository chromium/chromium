// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_preview/file_preview_controller.h"

#include <memory>

#include "ash/public/cpp/file_preview/file_preview_image_skia_source.h"
#include "base/callback_list.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

FilePreviewController::FilePreviewController(base::FilePath path,
                                             const gfx::Size& size)
    : source_(new FilePreviewImageSkiaSource(this, std::move(path))),
      image_skia_(gfx::ImageSkia(base::WrapUnique(source_.get()), size)),
      key_(GetKey(image_skia_)) {}

FilePreviewController::~FilePreviewController() {
  // Deletes the pointer to this controller in `source_` to prevent UAF. Also
  // lets `source_` stop doing any unnecessary work, since without a controller,
  // we have no method of removing existing reps to allow invalidation to work.
  source_->Shutdown();
}

// static
FilePreviewController::Key FilePreviewController::GetKey(
    const gfx::ImageSkia& image_skia) {
  return image_skia.GetBackingObject();
}

base::CallbackListSubscription FilePreviewController::AddInvalidationCallback(
    base::RepeatingClosure callback) {
  return invalidation_callbacks_.Add(callback);
}

void FilePreviewController::SetPlaybackMode(PlaybackMode playback_mode) {
  source_->SetPlaybackMode(playback_mode);
}

void FilePreviewController::Invalidate(
    base::PassKey<FilePreviewImageSkiaSource>) {
  // Remove all existing reps so that `ui::ImageModel::Rasterize()` will
  // generate new ones from the `FilePreviewImageSkiaSource`.
  for (auto& rep : image_skia_.image_reps()) {
    image_skia_.RemoveRepresentation(rep.scale());
  }
  invalidation_callbacks_.Notify();
}

const gfx::ImageSkia& FilePreviewController::GetImageSkiaForTest() const {
  // This check is to protect against regression, since the entire file preview
  // design hinges on the key being consistent.
  CHECK(GetKey(image_skia_) == key_);
  return image_skia_;
}

}  // namespace ash
