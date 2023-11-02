// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/lcd_text_metrics_reporter.h"

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "cc/base/histograms.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

namespace {

constexpr auto kMinimumTimeInterval = base::Minutes(1);
constexpr unsigned kMinimumFrameInterval = 500;

// This must be the same as that used in DeviceScaleEnsuresTextQuality() in
// content/renderer/render_widget.cc.
constexpr float kHighDPIDeviceScaleFactorThreshold = 1.5f;
constexpr char kMetricNameLCDTextKPixelsHighDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonKPixels.HighDPI";
constexpr char kMetricNameLCDTextKPixelsLowDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonKPixels.LowDPI";
constexpr char kMetricNameLCDTextLayersHighDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonLayers.HighDPI";
constexpr char kMetricNameLCDTextLayersLowDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonLayers.LowDPI";

}  // anonymous namespace

std::unique_ptr<LCDTextMetricsReporter> LCDTextMetricsReporter::CreateIfNeeded(
    const LayerTreeHostImpl* layer_tree_host_impl) {
  const char* client_name = GetClientNameForMetrics();
  // The metrics are for the renderer only.
  if (!client_name || strcmp(client_name, "Renderer") != 0)
    return nullptr;
  return base::WrapUnique(new LCDTextMetricsReporter(layer_tree_host_impl));
}

LCDTextMetricsReporter::LCDTextMetricsReporter(
    const LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl) {}

LCDTextMetricsReporter::~LCDTextMetricsReporter() = default;

void LCDTextMetricsReporter::NotifySubmitFrame(
    const viz::BeginFrameArgs& args) {
  current_frame_time_ = args.frame_time;
  frame_count_since_last_report_++;
  if (last_report_frame_time_.is_null())
    last_report_frame_time_ = current_frame_time_;
}

void LCDTextMetricsReporter::NotifyPauseFrameProduction() {
  if (current_frame_time_.is_null() ||
      current_frame_time_ - last_report_frame_time_ < kMinimumTimeInterval ||
      frame_count_since_last_report_ < kMinimumFrameInterval) {
    return;
  }

  last_report_frame_time_ = current_frame_time_;
  frame_count_since_last_report_ = 0;

  float device_scale_factor =
      layer_tree_host_impl_->settings().use_painted_device_scale_factor
          ? layer_tree_host_impl_->active_tree()->painted_device_scale_factor()
          : layer_tree_host_impl_->active_tree()->device_scale_factor();
  bool is_high_dpi = device_scale_factor >= kHighDPIDeviceScaleFactorThreshold;

  for (const auto* layer :
       layer_tree_host_impl_->active_tree()->picture_layers()) {
    if (!layer->draws_content() || !layer->GetRasterSource())
      continue;
    const scoped_refptr<DisplayItemList>& display_item_list =
        layer->GetRasterSource()->GetDisplayItemList();
    if (!display_item_list)
      continue;

    int text_pixels = static_cast<int>(
        display_item_list->AreaOfDrawText(layer->visible_layer_rect()));
    if (!text_pixels)
      continue;

    auto reason = layer->lcd_text_disallowed_reason();
    if (is_high_dpi) {
      UMA_HISTOGRAM_SCALED_ENUMERATION(kMetricNameLCDTextKPixelsHighDPI, reason,
                                       text_pixels, 1000);
      UMA_HISTOGRAM_ENUMERATION(kMetricNameLCDTextLayersHighDPI, reason);
    } else {
      UMA_HISTOGRAM_SCALED_ENUMERATION(kMetricNameLCDTextKPixelsLowDPI, reason,
                                       text_pixels, 1000);
      UMA_HISTOGRAM_ENUMERATION(kMetricNameLCDTextLayersLowDPI, reason);
    }
  }
}

}  // namespace cc
