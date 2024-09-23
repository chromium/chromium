// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager_client.h"

#include "cc/tiles/tiles_with_resource_iterator.h"

namespace cc {

FakeTileManagerClient::FakeTileManagerClient() = default;

FakeTileManagerClient::~FakeTileManagerClient() = default;

std::unique_ptr<RasterTilePriorityQueue>
FakeTileManagerClient::BuildRasterQueue(TreePriority tree_priority,
                                        RasterTilePriorityQueue::Type type) {
  return nullptr;
}

std::unique_ptr<EvictionTilePriorityQueue>
FakeTileManagerClient::BuildEvictionQueue(TreePriority tree_priority) {
  return nullptr;
}

std::unique_ptr<TilesWithResourceIterator>
FakeTileManagerClient::CreateTilesWithResourceIterator() {
  return nullptr;
}

TargetColorParams FakeTileManagerClient::GetTargetColorParams(
    gfx::ContentColorUsage /*content_color_usage*/) const {
  TargetColorParams result;
  result.color_space = color_space_;
  result.sdr_max_luminance_nits = gfx::ColorSpace::kDefaultSDRWhiteLevel;
  result.hdr_max_luminance_relative = 1.f;
  return result;
}

size_t FakeTileManagerClient::GetFrameIndexForImage(
    const PaintImage& paint_image,
    WhichTree tree) const {
  return PaintImage::kDefaultFrameIndex;
}

int FakeTileManagerClient::GetMSAASampleCountForRaster(
    const DisplayItemList& display_list) const {
  return 0;
}

bool FakeTileManagerClient::HasPendingTree() {
  return true;
}

}  // namespace cc
