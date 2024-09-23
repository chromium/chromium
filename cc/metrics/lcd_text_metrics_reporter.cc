// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/lcd_text_metrics_reporter.h"

#include "base/functional/function_ref.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
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
    "Compositing.Renderer.LCDTextDisallowedReasonKPixels2.HighDPI";
constexpr char kMetricNameLCDTextKPixelsLowDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonKPixels2.LowDPI";
constexpr char kMetricNameLCDTextLayersHighDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonLayers2.HighDPI";
constexpr char kMetricNameLCDTextLayersLowDPI[] =
    "Compositing.Renderer.LCDTextDisallowedReasonLayers2.LowDPI";

void Report(const LayerTreeImpl* layer_tree,
            base::FunctionRef<void(int64_t text_pixels,
                                   LCDTextDisallowedReason)> report_layer) {
  for (const PictureLayerImpl* layer : layer_tree->picture_layers()) {
    if (!layer->draws_content() || !layer->GetRasterSource()) {
      continue;
    }
    const scoped_refptr<const DisplayItemList>& display_item_list =
        layer->GetRasterSource()->GetDisplayItemList();
    if (!display_item_list) {
      continue;
    }

    int64_t text_pixels = base::checked_cast<int64_t>(
        display_item_list->AreaOfDrawText(layer->visible_layer_rect()));
    if (!text_pixels) {
      continue;
    }

    DCHECK_GT(text_pixels, 0);
    report_layer(text_pixels, layer->lcd_text_disallowed_reason());
  }
}

constexpr char const* kTraceCategory =
    TRACE_DISABLED_BY_DEFAULT("cc.debug.lcd_text");

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
  if (last_report_frame_time_.is_null()) {
    last_report_frame_time_ = current_frame_time_;
  }

  bool trace_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &trace_enabled);
  if (trace_enabled) {
    Report(layer_tree_host_impl_->active_tree(),
           [](int64_t text_pixels, LCDTextDisallowedReason reason) {
             TRACE_COUNTER2(kTraceCategory,
                            LCDTextDisallowedReasonToString(reason),
                            "text_pixels", text_pixels, "layers", 1);
           });
  }
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

  Report(layer_tree_host_impl_->active_tree(),
         [is_high_dpi](int64_t text_pixels, LCDTextDisallowedReason reason) {
           if (is_high_dpi) {
             UMA_HISTOGRAM_SCALED_ENUMERATION(kMetricNameLCDTextKPixelsHighDPI,
                                              reason, text_pixels, 1000);
             UMA_HISTOGRAM_ENUMERATION(kMetricNameLCDTextLayersHighDPI, reason);
           } else {
             UMA_HISTOGRAM_SCALED_ENUMERATION(kMetricNameLCDTextKPixelsLowDPI,
                                              reason, text_pixels, 1000);
             UMA_HISTOGRAM_ENUMERATION(kMetricNameLCDTextLayersLowDPI, reason);
           }
         });
}

}  // namespace cc
