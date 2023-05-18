// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CAPTURE_TRACKER_H_
#define CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CAPTURE_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace thumbnail {

// Tracks the status of a thumbnail as it is captured and written to
// disk. Must be created, checked and updated on a single thread.
class ThumbnailCaptureTracker {
 public:
  // `on_deleted` is invoked when the ThumbnailCaptureTracker is finished.
  explicit ThumbnailCaptureTracker(
      base::OnceCallback<void(ThumbnailCaptureTracker*)> on_deleted);
  ~ThumbnailCaptureTracker();

  ThumbnailCaptureTracker(const ThumbnailCaptureTracker&) = delete;
  ThumbnailCaptureTracker& operator=(const ThumbnailCaptureTracker&) = delete;

  // Set a callback to be run once a JPEG is available.
  void AddOnJpegFinishedCallback(
      base::OnceCallback<void(bool)> on_jpeg_finished);

  // Mark this thumbnail as having written its JPEG to disk.
  void SetWroteJpeg();

  // Marks this thumbnail as having failed during capture.
  void MarkCaptureFailed();

  // Marks this thumbnail as having failed during JPEG creation.
  void MarkJpegFailed();

  base::WeakPtr<ThumbnailCaptureTracker> GetWeakPtr();

 private:
  void RunOnJpegFinishedCallbacks(bool success);

  base::OnceCallback<void(ThumbnailCaptureTracker*)> on_deleted_;
  std::vector<base::OnceCallback<void(bool)>> on_jpeg_finished_callbacks_;
  bool wrote_jpeg_{false};
  bool capture_failed_{false};
  bool jpeg_failed_{false};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ThumbnailCaptureTracker> weak_ptr_factory_{this};
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_CAPTURE_TRACKER_H_
