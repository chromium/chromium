// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/frame_viewer_instrumentation.h"

#include <memory>
#include <utility>

#include "components/viz/common/traced_value.h"

namespace cc {
namespace frame_viewer_instrumentation {

namespace {

constexpr const char kCategory[] =
    "cc," TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
const char kTileData[] = "tileData";
const char kLayerId[] = "layerId";
const char kTileId[] = "tileId";
const char kTileResolution[] = "tileResolution";
const char kSourceFrameNumber[] = "sourceFrameNumber";

const char kAnalyzeTask[] = "AnalyzeTask";
const char kRasterTask[] = "RasterTask";

std::unique_ptr<base::trace_event::ConvertableToTraceFormat> TileDataAsValue(
    const void* tile_id,
    TileResolution tile_resolution,
    int source_frame_number,
    int layer_id) {
  std::unique_ptr<base::trace_event::TracedValue> res(
      new base::trace_event::TracedValue());
  viz::TracedValue::SetIDRef(tile_id, res.get(), kTileId);
  res->SetString(kTileResolution, TileResolutionToString(tile_resolution));
  res->SetInteger(kSourceFrameNumber, source_frame_number);
  res->SetInteger(kLayerId, layer_id);
  return std::move(res);
}

}  // namespace

ScopedAnalyzeTask::ScopedAnalyzeTask(const void* tile_id,
                                     TileResolution tile_resolution,
                                     int source_frame_number,
                                     int layer_id) {
  TRACE_EVENT_BEGIN1(
      kCategory, kAnalyzeTask, kTileData,
      TileDataAsValue(tile_id, tile_resolution, source_frame_number, layer_id));
}

ScopedAnalyzeTask::~ScopedAnalyzeTask() {
  TRACE_EVENT_END0(kCategory, kAnalyzeTask);
}

ScopedRasterTask::ScopedRasterTask(const void* tile_id,
                                   TileResolution tile_resolution,
                                   int source_frame_number,
                                   int layer_id) {
  TRACE_EVENT_BEGIN1(
      kCategory, kRasterTask, kTileData,
      TileDataAsValue(tile_id, tile_resolution, source_frame_number, layer_id));
}

ScopedRasterTask::~ScopedRasterTask() {
  TRACE_EVENT_END0(kCategory, kRasterTask);
}

bool IsTracingLayerTreeSnapshots() {
  bool category_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(CategoryLayerTree(), &category_enabled);
  return category_enabled;
}

}  // namespace frame_viewer_instrumentation
}  // namespace cc
