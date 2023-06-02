// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail_capture_tracker.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace thumbnail {

ThumbnailCaptureTracker::ThumbnailCaptureTracker(
    base::OnceCallback<void(ThumbnailCaptureTracker*)> on_deleted)
    : on_deleted_(std::move(on_deleted)) {}

ThumbnailCaptureTracker::~ThumbnailCaptureTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There may be callbacks left if this was interrupted.
  if (wrote_jpeg_) {
    RunOnJpegFinishedCallbacks(true);
  } else {
    RunOnJpegFinishedCallbacks(false);
  }
  std::move(on_deleted_).Run(this);
}

void ThumbnailCaptureTracker::AddOnJpegFinishedCallback(
    base::OnceCallback<void(bool)> on_jpeg_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (wrote_jpeg_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_jpeg_finished), true));
  } else if (jpeg_failed_ || capture_failed_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_jpeg_finished), false));
  } else {
    on_jpeg_finished_callbacks_.push_back(std::move(on_jpeg_finished));
  }
}

void ThumbnailCaptureTracker::SetWroteJpeg() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wrote_jpeg_ = true;
  RunOnJpegFinishedCallbacks(true);
}

void ThumbnailCaptureTracker::MarkCaptureFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capture_failed_ = true;
  RunOnJpegFinishedCallbacks(false);
}

void ThumbnailCaptureTracker::MarkJpegFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  jpeg_failed_ = true;
  RunOnJpegFinishedCallbacks(false);
}

base::WeakPtr<ThumbnailCaptureTracker> ThumbnailCaptureTracker::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void ThumbnailCaptureTracker::RunOnJpegFinishedCallbacks(bool success) {
  for (auto& callback : on_jpeg_finished_callbacks_) {
    std::move(callback).Run(success);
  }
  on_jpeg_finished_callbacks_.clear();
}

}  // namespace thumbnail
