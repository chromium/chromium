// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include "components/viz/common/resources/platform_color.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

LayerTreeSettings::LayerTreeSettings()
    : default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      memory_policy(64 * 1024 * 1024,
                    gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                    ManagedMemoryPolicy::kDefaultNumResourcesLimit) {}

LayerTreeSettings::LayerTreeSettings(const LayerTreeSettings& other) = default;
LayerTreeSettings::~LayerTreeSettings() = default;

SchedulerSettings LayerTreeSettings::ToSchedulerSettings() const {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled =
      main_frame_before_activation_enabled;
  scheduler_settings.using_synchronous_renderer_compositor =
      using_synchronous_renderer_compositor;
  scheduler_settings.wait_for_all_pipeline_stages_before_draw =
      wait_for_all_pipeline_stages_before_draw;
  scheduler_settings.disable_frame_rate_limit = disable_frame_rate_limit;
  return scheduler_settings;
}

TileManagerSettings LayerTreeSettings::ToTileManagerSettings() const {
  TileManagerSettings tile_manager_settings;
  tile_manager_settings.use_partial_raster = use_partial_raster;
  tile_manager_settings.enable_checker_imaging = enable_checker_imaging;
  tile_manager_settings.min_image_bytes_to_checker = min_image_bytes_to_checker;
  tile_manager_settings.needs_notify_ready_to_draw =
      commit_to_active_tree | wait_for_all_pipeline_stages_before_draw;
  return tile_manager_settings;
}

}  // namespace cc
