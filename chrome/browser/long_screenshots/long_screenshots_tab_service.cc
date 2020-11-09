// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"

#include "base/callback.h"

namespace long_screenshots {

LongScreenshotsTabService::LongScreenshotsTabService(
    const base::FilePath& profile_dir,
    base::StringPiece ascii_feature_name,
    std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
    bool is_off_the_record)

    : PaintPreviewBaseService(profile_dir,
                              ascii_feature_name,
                              std::move(policy),
                              is_off_the_record) {
  // TODO(tgupta): Populate this.
}

LongScreenshotsTabService::~LongScreenshotsTabService() {
  // TODO(tgupta): Populate this.
}

void LongScreenshotsTabService::CaptureTab(int tab_id,
                                           content::WebContents* contents,
                                           FinishedCallback callback) {
  // TODO(tgupta): Populate this.
}

void LongScreenshotsTabService::CaptureTabInternal(
    int tab_id,
    const paint_preview::DirectoryKey& key,
    int frame_tree_node_id,
    content::GlobalFrameRoutingId frame_routing_id,
    FinishedCallback callback,
    const base::Optional<base::FilePath>& file_path) {
  // TODO(tgupta): Complete this function
}

void LongScreenshotsTabService::OnCaptured(
    int tab_id,
    const paint_preview::DirectoryKey& key,
    int frame_tree_node_id,
    FinishedCallback callback,
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  // TODO(tgupta): Populate this.
}

void LongScreenshotsTabService::OnFinished(int tab_id,
                                           FinishedCallback callback,
                                           bool success) {
  // TODO(tgupta): Complete this function
}

}  // namespace long_screenshots
