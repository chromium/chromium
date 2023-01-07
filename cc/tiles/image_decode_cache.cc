// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_decode_cache.h"

#include "cc/raster/tile_task.h"

namespace cc {

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

}  // namespace cc
