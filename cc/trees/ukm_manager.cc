// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/ukm_manager.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

UkmManager::UkmManager(std::unique_ptr<ukm::UkmRecorder> recorder)
    : recorder_(std::move(recorder)) {
  DCHECK(recorder_);
}

UkmManager::~UkmManager() {
  RecordCheckerboardUkm();
  RecordRenderingUkm();
}

void UkmManager::SetSourceId(ukm::SourceId source_id) {
  // If we accumulated any metrics, record them before resetting the source.
  RecordCheckerboardUkm();
  RecordRenderingUkm();

  source_id_ = source_id;
}

void UkmManager::SetUserInteractionInProgress(bool in_progress) {
  if (user_interaction_in_progress_ == in_progress)
    return;

  user_interaction_in_progress_ = in_progress;
  if (!user_interaction_in_progress_)
    RecordCheckerboardUkm();
}

void UkmManager::AddCheckerboardStatsForFrame(int64_t checkerboard_area,
                                              int64_t num_missing_tiles,
                                              int64_t total_visible_area) {
  DCHECK_GE(total_visible_area, checkerboard_area);
  if (source_id_ == ukm::kInvalidSourceId || !user_interaction_in_progress_)
    return;

  checkerboarded_content_area_ += checkerboard_area;
  num_missing_tiles_ += num_missing_tiles;
  total_visible_area_ += total_visible_area;
  num_of_frames_++;
}

void UkmManager::AddCheckerboardedImages(int num_of_checkerboarded_images) {
  if (user_interaction_in_progress_) {
    num_of_images_checkerboarded_during_interaction_ +=
        num_of_checkerboarded_images;
  }
  total_num_of_checkerboarded_images_ += num_of_checkerboarded_images;
}

void UkmManager::RecordCheckerboardUkm() {
  // Only make a recording if there was any visible area from PictureLayers,
  // which can be checkerboarded.
  if (num_of_frames_ > 0 && total_visible_area_ > 0) {
    DCHECK_NE(source_id_, ukm::kInvalidSourceId);
    ukm::builders::Compositor_UserInteraction(source_id_)
        .SetCheckerboardedContentArea(checkerboarded_content_area_ /
                                      num_of_frames_)
        .SetNumMissingTiles(num_missing_tiles_ / num_of_frames_)
        .SetCheckerboardedContentAreaRatio(
            (checkerboarded_content_area_ * 100) / total_visible_area_)
        .SetCheckerboardedImagesCount(
            num_of_images_checkerboarded_during_interaction_)
        .Record(recorder_.get());
  }

  checkerboarded_content_area_ = 0;
  num_missing_tiles_ = 0;
  num_of_frames_ = 0;
  total_visible_area_ = 0;
  num_of_images_checkerboarded_during_interaction_ = 0;
}

void UkmManager::RecordRenderingUkm() {
  if (source_id_ == ukm::kInvalidSourceId)
    return;

  ukm::builders::Compositor_Rendering(source_id_)
      .SetCheckerboardedImagesCount(total_num_of_checkerboarded_images_)
      .Record(recorder_.get());
  total_num_of_checkerboarded_images_ = 0;
}

}  // namespace cc
