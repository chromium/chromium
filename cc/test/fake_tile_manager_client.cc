// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager_client.h"

#include "cc/tiles/occluded_tile_iterator.h"

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

std::unique_ptr<OccludedTileIterator>
FakeTileManagerClient::CreateOccludedTileIterator() {
  return nullptr;
}

gfx::ColorSpace FakeTileManagerClient::GetRasterColorSpace(
    gfx::ContentColorUsage /*content_color_usage*/) const {
  return color_space_;
}

float FakeTileManagerClient::GetSDRWhiteLevel() const {
  return gfx::ColorSpace::kDefaultSDRWhiteLevel;
}

size_t FakeTileManagerClient::GetFrameIndexForImage(
    const PaintImage& paint_image,
    WhichTree tree) const {
  return PaintImage::kDefaultFrameIndex;
}

int FakeTileManagerClient::GetMSAASampleCountForRaster(
    const scoped_refptr<DisplayItemList>& display_list) {
  return 0;
}

bool FakeTileManagerClient::HasPendingTree() {
  return true;
}

}  // namespace cc
