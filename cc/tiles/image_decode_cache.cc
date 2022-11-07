// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_decode_cache.h"

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "cc/raster/tile_task.h"

namespace cc {

const ImageDecodeCache::ClientId ImageDecodeCache::kDefaultClientId = 1;

ImageDecodeCache::TaskResult::TaskResult(
    bool need_unref,
    bool is_at_raster_decode,
    bool can_do_hardware_accelerated_decode)
    : need_unref(need_unref),
      is_at_raster_decode(is_at_raster_decode),
      can_do_hardware_accelerated_decode(can_do_hardware_accelerated_decode) {}

ImageDecodeCache::TaskResult::TaskResult(
    scoped_refptr<TileTask> task,
    bool can_do_hardware_accelerated_decode)
    : task(std::move(task)),
      need_unref(true),
      is_at_raster_decode(false),
      can_do_hardware_accelerated_decode(can_do_hardware_accelerated_decode) {
  DCHECK(this->task);
}

ImageDecodeCache::TaskResult::TaskResult(const TaskResult& result) = default;

ImageDecodeCache::TaskResult::~TaskResult() = default;

ImageDecodeCache::ClientId ImageDecodeCache::GenerateClientId() {
  DCHECK_LT(next_available_id_, std::numeric_limits<uint32_t>::max());
  return ++next_available_id_;
}

}  // namespace cc
