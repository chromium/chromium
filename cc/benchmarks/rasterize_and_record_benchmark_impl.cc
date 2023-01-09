// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/rasterize_and_record_benchmark_impl.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/lap_timer.h"
#include "base/values.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/display_item_list.h"
#include "cc/raster/playback_image_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/legacy_display_globals.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

namespace {


void RunBenchmark(RasterSource* raster_source,
                  ImageDecodeCache* image_decode_cache,
                  const gfx::Rect& content_rect,
                  const gfx::Vector2dF& contents_scale,
                  size_t repeat_count,
                  base::TimeDelta* min_time,
                  bool* is_solid_color) {
  // Parameters for base::LapTimer.
  const int kTimeLimitMillis = 1;
  const int kWarmupRuns = 0;
  const int kTimeCheckInterval = 1;

  *min_time = base::TimeDelta::Max();
  for (size_t i = 0; i < repeat_count; ++i) {
    // Run for a minimum amount of time to avoid problems with timer
    // quantization when the layer is very small.
    base::LapTimer timer(kWarmupRuns, base::Milliseconds(kTimeLimitMillis),
                         kTimeCheckInterval);
    SkColor4f color = SkColors::kTransparent;
    gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
        content_rect, 1.f / contents_scale.x(), 1.f / contents_scale.y());
    *is_solid_color =
        raster_source->PerformSolidColorAnalysis(layer_rect, &color);

    do {
      SkBitmap bitmap;
      bitmap.allocPixels(SkImageInfo::MakeN32Premul(content_rect.width(),
                                                    content_rect.height()));
      SkCanvas canvas(bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());

      // Pass an empty settings to make sure that the decode cache is used to
      // replace all images.
      absl::optional<PlaybackImageProvider::Settings> image_settings;
      image_settings.emplace();
      image_settings->images_to_skip = {};
      image_settings->image_to_current_frame_index = {};

      PlaybackImageProvider image_provider(
          image_decode_cache, TargetColorParams(), std::move(image_settings));
      RasterSource::PlaybackSettings settings;
      settings.image_provider = &image_provider;

      raster_source->PlaybackToCanvas(
          &canvas, raster_source->GetContentSize(contents_scale), content_rect,
          content_rect,
          gfx::AxisTransform2d::FromScaleAndTranslation(contents_scale,
                                                        gfx::Vector2dF()),
          settings);

      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());
    base::TimeDelta duration = timer.TimePerLap();
    if (duration < *min_time)
      *min_time = duration;
  }
}

class FixedInvalidationPictureLayerTilingClient
    : public PictureLayerTilingClient {
 public:
  FixedInvalidationPictureLayerTilingClient(
      PictureLayerTilingClient* base_client,
      const Region& invalidation)
      : base_client_(base_client), invalidation_(invalidation) {}

  std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) override {
    return base_client_->CreateTile(info);
  }

  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) override {
    return base_client_->CalculateTileSize(content_bounds);
  }

  // This is the only function that returns something different from the base
  // client. Avoids sharing tiles in this area.
  const Region* GetPendingInvalidation() override { return &invalidation_; }

  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override {
    return base_client_->GetPendingOrActiveTwinTiling(tiling);
  }

  bool HasValidTilePriorities() const override {
    return base_client_->HasValidTilePriorities();
  }

  bool RequiresHighResToDraw() const override {
    return base_client_->RequiresHighResToDraw();
  }

  const PaintWorkletRecordMap& GetPaintWorkletRecords() const override {
    return base_client_->GetPaintWorkletRecords();
  }

  bool IsDirectlyCompositedImage() const override {
    return base_client_->IsDirectlyCompositedImage();
  }

  bool ScrollInteractionInProgress() const override {
    return base_client_->ScrollInteractionInProgress();
  }

  bool CurrentScrollCheckerboardsDueToNoRecording() const override {
    return base_client_->CurrentScrollCheckerboardsDueToNoRecording();
  }

 private:
  raw_ptr<PictureLayerTilingClient> base_client_;
  Region invalidation_;
};

}  // namespace

RasterizeAndRecordBenchmarkImpl::RasterizeAndRecordBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
    int rasterize_repeat_count,
    MicroBenchmarkImpl::DoneCallback callback)
    : MicroBenchmarkImpl(std::move(callback), origin_task_runner),
      rasterize_repeat_count_(rasterize_repeat_count) {}

RasterizeAndRecordBenchmarkImpl::~RasterizeAndRecordBenchmarkImpl() = default;

void RasterizeAndRecordBenchmarkImpl::DidCompleteCommit(
    LayerTreeHostImpl* host) {
  for (auto* layer : *host->active_tree()) {
    rasterize_results_.total_layers++;
    layer->RunMicroBenchmark(this);
  }

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetDoubleKey("rasterize_time_ms",
                      rasterize_results_.total_best_time.InMillisecondsF());
  result.SetIntKey("pixels_rasterized", rasterize_results_.pixels_rasterized);
  result.SetIntKey("pixels_rasterized_with_non_solid_color",
                   rasterize_results_.pixels_rasterized_with_non_solid_color);
  result.SetIntKey("pixels_rasterized_as_opaque",
                   rasterize_results_.pixels_rasterized_as_opaque);
  result.SetIntKey("total_layers", rasterize_results_.total_layers);
  result.SetIntKey("total_picture_layers",
                   rasterize_results_.total_picture_layers);
  result.SetIntKey("total_picture_layers_with_no_content",
                   rasterize_results_.total_picture_layers_with_no_content);
  result.SetIntKey("total_picture_layers_off_screen",
                   rasterize_results_.total_picture_layers_off_screen);

  base::Value lcd_text_pixels(base::Value::Type::DICTIONARY);
  for (size_t i = 0; i < kLCDTextDisallowedReasonCount; i++) {
    lcd_text_pixels.SetIntKey(
        LCDTextDisallowedReasonToString(
            static_cast<LCDTextDisallowedReason>(i)),
        rasterize_results_.visible_pixels_by_lcd_text_disallowed_reason[i]);
  }
  result.SetKey("visible_pixels_by_lcd_text_disallowed_reason",
                std::move(lcd_text_pixels));

  NotifyDone(std::move(result));
}

void RasterizeAndRecordBenchmarkImpl::RunOnLayer(PictureLayerImpl* layer) {
  rasterize_results_.total_picture_layers++;
  if (!layer->CanHaveTilings()) {
    rasterize_results_.total_picture_layers_with_no_content++;
    return;
  }
  if (layer->visible_layer_rect().IsEmpty()) {
    rasterize_results_.total_picture_layers_off_screen++;
    return;
  }

  if (layer->ShouldAdjustRasterScale())
    layer->RecalculateRasterScales();

  int text_pixels =
      layer->GetRasterSource()->GetDisplayItemList()->AreaOfDrawText(
          layer->visible_layer_rect());
  rasterize_results_
      .visible_pixels_by_lcd_text_disallowed_reason[static_cast<size_t>(
          layer->lcd_text_disallowed_reason())] += text_pixels;

  FixedInvalidationPictureLayerTilingClient client(layer,
                                                   gfx::Rect(layer->bounds()));

  // In this benchmark, we will create a local tiling set and measure how long
  // it takes to rasterize content. As such, the actual settings used here don't
  // really matter.
  const LayerTreeSettings& settings = layer->layer_tree_impl()->settings();
  std::unique_ptr<PictureLayerTilingSet> tiling_set =
      PictureLayerTilingSet::Create(
          layer->IsActive() ? ACTIVE_TREE : PENDING_TREE, &client,
          settings.tiling_interest_area_padding,
          settings.skewport_target_time_in_seconds,
          settings.skewport_extrapolation_limit_in_screen_pixels,
          settings.max_preraster_distance_in_screen_pixels);

  PictureLayerTiling* tiling = tiling_set->AddTiling(
      gfx::AxisTransform2d::FromScaleAndTranslation(
          layer->raster_contents_scale_, gfx::Vector2dF()),
      layer->GetRasterSource());
  tiling->set_resolution(HIGH_RESOLUTION);
  tiling->CreateAllTilesForTesting();
  RasterSource* raster_source = tiling->raster_source().get();
  for (PictureLayerTiling::CoverageIterator it(
           tiling, tiling->contents_scale_key(), layer->visible_layer_rect());
       it; ++it) {
    DCHECK(*it);

    gfx::Rect content_rect = (*it)->content_rect();
    const gfx::Vector2dF& contents_scale = (*it)->raster_transform().scale();

    base::TimeDelta min_time;
    bool is_solid_color = false;
    RunBenchmark(raster_source, layer->layer_tree_impl()->image_decode_cache(),
                 content_rect, contents_scale, rasterize_repeat_count_,
                 &min_time, &is_solid_color);

    int tile_size = content_rect.width() * content_rect.height();
    if (layer->contents_opaque())
      rasterize_results_.pixels_rasterized_as_opaque += tile_size;

    if (!is_solid_color)
      rasterize_results_.pixels_rasterized_with_non_solid_color += tile_size;

    rasterize_results_.pixels_rasterized += tile_size;
    rasterize_results_.total_best_time += min_time;
  }
}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::RasterizeResults()
    : pixels_rasterized(0),
      pixels_rasterized_with_non_solid_color(0),
      pixels_rasterized_as_opaque(0),
      visible_pixels_by_lcd_text_disallowed_reason{0},
      total_layers(0),
      total_picture_layers(0),
      total_picture_layers_with_no_content(0),
      total_picture_layers_off_screen(0) {}

RasterizeAndRecordBenchmarkImpl::RasterizeResults::~RasterizeResults() =
    default;

}  // namespace cc
