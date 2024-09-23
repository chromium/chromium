// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/memory_graph_page_view.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/reference_lines.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace hud_display {

////////////////////////////////////////////////////////////////////////////////
// MemoryGraphPageView, public:

BEGIN_METADATA(MemoryGraphPageView)
END_METADATA

MemoryGraphPageView::MemoryGraphPageView(const base::TimeDelta refresh_interval)
    : graph_chrome_rss_private_(kHUDGraphWidth,
                                Graph::Baseline::kBaselineBottom,
                                Graph::Fill::kSolid,
                                Graph::Style::kLines,
                                SkColorSetA(SK_ColorRED, kHUDAlpha)),
      graph_mem_free_(kHUDGraphWidth,
                      Graph::Baseline::kBaselineBottom,
                      Graph::Fill::kNone,
                      Graph::Style::kLines,
                      SkColorSetA(SK_ColorDKGRAY, kHUDAlpha)),
      graph_mem_used_unknown_(kHUDGraphWidth,
                              Graph::Baseline::kBaselineBottom,
                              Graph::Fill::kSolid,
                              Graph::Style::kLines,
                              SkColorSetA(SK_ColorLTGRAY, kHUDAlpha)),
      graph_renderers_rss_private_(kHUDGraphWidth,
                                   Graph::Baseline::kBaselineBottom,
                                   Graph::Fill::kSolid,
                                   Graph::Style::kLines,
                                   SkColorSetA(SK_ColorCYAN, kHUDAlpha)),
      graph_arc_rss_private_(kHUDGraphWidth,
                             Graph::Baseline::kBaselineBottom,
                             Graph::Fill::kSolid,
                             Graph::Style::kLines,
                             SkColorSetA(SK_ColorMAGENTA, kHUDAlpha)),
      graph_gpu_rss_private_(kHUDGraphWidth,
                             Graph::Baseline::kBaselineBottom,
                             Graph::Fill::kSolid,
                             Graph::Style::kLines,
                             SkColorSetA(SK_ColorRED, kHUDAlpha)),
      graph_gpu_kernel_(kHUDGraphWidth,
                        Graph::Baseline::kBaselineBottom,
                        Graph::Fill::kSolid,
                        Graph::Style::kLines,
                        SkColorSetA(SK_ColorYELLOW, kHUDAlpha)),
      graph_chrome_rss_shared_(kHUDGraphWidth,
                               Graph::Baseline::kBaselineBottom,
                               Graph::Fill::kNone,
                               Graph::Style::kLines,
                               SkColorSetA(SK_ColorBLUE, kHUDAlpha)) {
  const int data_width = graph_arc_rss_private_.max_data_points();
  // Verical ticks are drawn every 10% (10/100 interval).
  constexpr float vertical_ticks_interval = 10 / 100.0;
  // -XX seconds on the left, 0Gb top (will be updated later), 0 seconds on the
  // right, 0 Gb on the bottom. Seconds and Gigabytes are dimensions. Number of
  // data points is data_width. horizontal tick marks are drawn every 10
  // seconds.
  reference_lines_ = CreateReferenceLines(
      static_cast<int>(/*left=*/-data_width * refresh_interval.InSecondsF()),
      /*top=*/0, /*right=*/0, /*bottom=*/0, /*x_unit=*/u"s",
      /*y_unit=*/u"Gb",
      /*horizontal_points_number=*/data_width,
      /*horizontal_ticks_interval=*/10 / refresh_interval.InSecondsF(),
      vertical_ticks_interval);
  // Hide reference lines until we know total memory size.
  reference_lines_->SetVisible(false);

  Legend::Formatter formatter = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(
        base::StringPrintf("%d Mb", std::max(0, (int)(value * 1024))));
  });

  const std::vector<Legend::Entry> legend({
      {graph_gpu_kernel_, u"GPU Driver",
       u"Kernel GPU buffers as reported\nby base::SystemMemoryInfo::gem_size.",
       formatter},
      {graph_gpu_rss_private_, u"Chrome GPU",
       u"RSS private memory of\n --type=gpu-process Chrome process.",
       formatter},
      // ARC memory is not usually visible (skipped)
      {graph_renderers_rss_private_, u"Renderers",
       u"Sum of RSS private memory of\n--type=renderer Chrome process.",
       formatter},
      {graph_mem_used_unknown_, u"Other",
       u"Amount of other used memory.\nEquals to total used minus known.",
       formatter},
      {graph_mem_free_, u"Free", u"Free memory as reported by kernel.",
       formatter},
      {graph_chrome_rss_private_, u"Browser",
       u"RSS private memory of the\nmain Chrome process.", formatter}
      // Browser RSS hairline skipped.
  });
  CreateLegend(legend);
}

MemoryGraphPageView::~MemoryGraphPageView() = default;

////////////////////////////////////////////////////////////////////////////////

void MemoryGraphPageView::OnPaint(gfx::Canvas* canvas) {
  // TODO: Should probably update last graph point more often than shift graph.

  // Layout graphs.
  gfx::Rect rect = GetContentsBounds();
  // Adjust bounds to not overlap with bordering reference lines.
  rect.Inset(kHUDGraphReferenceLineWidth);
  graph_chrome_rss_private_.Layout(rect, /*base=*/nullptr);
  graph_mem_free_.Layout(rect, &graph_chrome_rss_private_);
  graph_mem_used_unknown_.Layout(rect, &graph_mem_free_);
  graph_renderers_rss_private_.Layout(rect, &graph_mem_used_unknown_);
  graph_arc_rss_private_.Layout(rect, &graph_renderers_rss_private_);
  graph_gpu_rss_private_.Layout(rect, &graph_arc_rss_private_);
  graph_gpu_kernel_.Layout(rect, &graph_gpu_rss_private_);
  // Not stacked.
  graph_chrome_rss_shared_.Layout(rect, /*base=*/nullptr);

  // Paint damaged area now that all parameters have been determined.
  graph_chrome_rss_private_.Draw(canvas);
  graph_mem_free_.Draw(canvas);
  graph_mem_used_unknown_.Draw(canvas);
  graph_renderers_rss_private_.Draw(canvas);
  graph_arc_rss_private_.Draw(canvas);
  graph_gpu_rss_private_.Draw(canvas);
  graph_gpu_kernel_.Draw(canvas);

  graph_chrome_rss_shared_.Draw(canvas);
}

void MemoryGraphPageView::UpdateData(const DataSource::Snapshot& snapshot) {
  // TODO: Should probably update last graph point more often than shift graph.
  const double total = snapshot.total_ram;
  // Nothing to do if data is not available yet.
  if (total < 1)
    return;

  constexpr float one_gigabyte = 1024 * 1024 * 1024;

  if (total_ram_ != total) {
    total_ram_ = total;
    reference_lines_->SetTopLabel(total / one_gigabyte);  // In Gigabytes.
    reference_lines_->SetVisible(true);
  }

  const float chrome_rss_private_unscaled =
      (snapshot.browser_rss - snapshot.browser_rss_shared);
  const float chrome_rss_private = chrome_rss_private_unscaled / total;
  const float mem_free_unscaled = snapshot.free_ram;
  const float mem_free = mem_free_unscaled / total;
  // mem_used_unknown is calculated below.
  const float renderers_rss_private_unscaled =
      snapshot.renderers_rss - snapshot.renderers_rss_shared;
  const float renderers_rss_private = renderers_rss_private_unscaled / total;
  const float arc_rss_private_unscaled =
      snapshot.arc_rss - snapshot.arc_rss_shared;
  const float arc_rss_private = arc_rss_private_unscaled / total;
  const float gpu_rss_private_unscaled =
      snapshot.gpu_rss - snapshot.gpu_rss_shared;
  const float gpu_rss_private = gpu_rss_private_unscaled / total;
  const float gpu_kernel_unscaled = snapshot.gpu_kernel;
  const float gpu_kernel = gpu_kernel_unscaled / total;

  // not stacked.
  const float chrome_rss_shared_unscaled = snapshot.browser_rss_shared;
  const float chrome_rss_shared = chrome_rss_shared_unscaled / total;

  std::vector<float> used_buckets;
  used_buckets.push_back(chrome_rss_private);
  used_buckets.push_back(mem_free);
  used_buckets.push_back(renderers_rss_private);
  used_buckets.push_back(arc_rss_private);
  used_buckets.push_back(gpu_rss_private);
  used_buckets.push_back(gpu_kernel);

  const float mem_used_unknown =
      1 - std::reduce(used_buckets.begin(), used_buckets.end(), 0.0f);
  const float mem_used_unknown_unscaled = mem_used_unknown * total;

  if (mem_used_unknown < 0)
    LOG(WARNING) << "mem_used_unknown=" << mem_used_unknown << " < 0 !";

  // Update graph data.
  graph_chrome_rss_private_.AddValue(
      chrome_rss_private, chrome_rss_private_unscaled / one_gigabyte);
  graph_mem_free_.AddValue(mem_free, mem_free_unscaled / one_gigabyte);
  graph_mem_used_unknown_.AddValue(
      std::max(mem_used_unknown, 0.0f),
      std::max(mem_used_unknown_unscaled / one_gigabyte, 0.0f));
  graph_renderers_rss_private_.AddValue(
      renderers_rss_private, renderers_rss_private_unscaled / one_gigabyte);
  graph_arc_rss_private_.AddValue(arc_rss_private,
                                  arc_rss_private_unscaled / one_gigabyte);
  graph_gpu_rss_private_.AddValue(gpu_rss_private,
                                  gpu_rss_private_unscaled / one_gigabyte);
  graph_gpu_kernel_.AddValue(gpu_kernel, gpu_kernel_unscaled / one_gigabyte);
  // Not stacked.
  graph_chrome_rss_shared_.AddValue(chrome_rss_shared,
                                    chrome_rss_shared_unscaled / one_gigabyte);

  RefreshLegendValues();
}

}  // namespace hud_display
}  // namespace ash
