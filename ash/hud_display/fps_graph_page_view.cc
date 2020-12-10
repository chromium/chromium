// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/fps_graph_page_view.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "ash/hud_display/grid.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {

namespace {
// Draw tick on the vertical axis every 5 frames.
constexpr float kVerticalTicFrames = 5;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FPSGraphPageView, public:

BEGIN_METADATA(FPSGraphPageView, GraphPageViewBase)
END_METADATA

FPSGraphPageView::FPSGraphPageView(const base::TimeDelta refresh_interval)
    : frame_rate_1s_(kDefaultGraphWidth,
                     Graph::Baseline::BASELINE_BOTTOM,
                     Graph::Fill::NONE,
                     Graph::Style::SKYLINE,
                     SkColorSetA(SK_ColorYELLOW, kHUDAlpha)),
      frame_rate_500ms_(kDefaultGraphWidth,
                        Graph::Baseline::BASELINE_BOTTOM,
                        Graph::Fill::NONE,
                        Graph::Style::SKYLINE,
                        SkColorSetA(SK_ColorCYAN, kHUDAlpha)),
      refresh_rate_(kDefaultGraphWidth,
                    Graph::Baseline::BASELINE_BOTTOM,
                    Graph::Fill::NONE,
                    Graph::Style::SKYLINE,
                    kHUDBackground /*not drawn*/) {
  const int data_width = frame_rate_1s_.max_data_points();
  // Verical ticks are drawn every 5 frames (5/60 interval).
  constexpr float vertical_ticks_interval = kVerticalTicFrames / 60.0;
  // max_data_points left label, 60fps  top, 0 seconds on the right, 0fps on the
  // bottom. Seconds and fps are dimensions. Number of data points is
  // data_width, horizontal grid ticks are drawn every 10 frames.
  grid_ = CreateGrid(
      /*left=*/data_width,
      /*top=*/60, /*right=*/0, /*bottom=*/0,
      /*x_unit=*/base::ASCIIToUTF16("frames"),
      /*y_unit=*/base::ASCIIToUTF16("fps"),
      /*horizontal_points_number=*/data_width,
      /*horizontal_ticks_interval=*/10, vertical_ticks_interval);

  Legend::Formatter formatter_float = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(base::StringPrintf("%.1f", value));
  });

  Legend::Formatter formatter_int = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(base::StringPrintf("%d", (int)value));
  });

  const std::vector<Legend::Entry> legend(
      {{refresh_rate_, base::ASCIIToUTF16("Refresh rate"),
        base::ASCIIToUTF16("Actual display refresh rate."), formatter_int},
       {frame_rate_1s_, base::ASCIIToUTF16("1s FPS"),
        base::ASCIIToUTF16(
            "Number of frames successfully presented per 1 second."),
        formatter_float},
       {frame_rate_500ms_, base::ASCIIToUTF16(".5s FPS"),
        base::ASCIIToUTF16("Number of frames successfully presented per 0.5 "
                           "second scaled to a second."),
        formatter_float}});
  CreateLegend(legend);
  AddObserver(this);
}

FPSGraphPageView::~FPSGraphPageView() {
  RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////

void FPSGraphPageView::OnPaint(gfx::Canvas* canvas) {
  // TODO: Should probably update last graph point more often than shift graph.

  // Layout graphs.
  gfx::Rect rect = GetContentsBounds();
  // Adjust to grid width.
  rect.Inset(kGridLineWidth, kGridLineWidth);

  frame_rate_500ms_.Layout(rect, /*base=*/nullptr);
  frame_rate_1s_.Layout(rect, /*base=*/nullptr);

  frame_rate_500ms_.Draw(canvas);
  frame_rate_1s_.Draw(canvas);
  // Refresh rate graph is not drawn, it's just used in Legend display and
  // grid calculations.
}

void FPSGraphPageView::OnDidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  UpdateStats(feedback);

  float frame_rate_1s = frame_rate_for_last_second();
  float frame_rate_500ms = frame_rate_for_last_half_second();

  float refresh_rate = GetWidget()->GetCompositor()->refresh_rate();

  UpdateTopLabel(refresh_rate);
  frame_rate_1s_.AddValue(frame_rate_1s / grid_->top_label(), frame_rate_1s);

  frame_rate_500ms_.AddValue(frame_rate_500ms / grid_->top_label(),
                             frame_rate_500ms);

  const float max_refresh_rate =
      std::max(refresh_rate, refresh_rate_.GetUnscaledValueAt(0));
  refresh_rate_.AddValue(max_refresh_rate / grid_->top_label(),
                         max_refresh_rate);
  // Legend update is expensive. Do it synchronously on regular intervals only.
  if (GetVisible())
    SchedulePaint();
}

void FPSGraphPageView::UpdateData(const DataSource::Snapshot& snapshot) {
  if (!GetWidget()->GetNativeWindow()->HasObserver(this)) {
    GetWidget()->GetNativeWindow()->AddObserver(this);
    GetWidget()->GetCompositor()->AddObserver(this);
  }
  // Graph moves only on FramePresented.
  // Update legend only.
  RefreshLegendValues();
}

void FPSGraphPageView::OnViewRemovedFromWidget(View* observed_view) {
  // Remove observe for destruction.
  GetWidget()->GetNativeWindow()->RemoveObserver(this);
  GetWidget()->GetCompositor()->RemoveObserver(this);
}

void FPSGraphPageView::OnWindowAddedToRootWindow(aura::Window* window) {
  GetWidget()->GetCompositor()->AddObserver(this);
}

void FPSGraphPageView::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                      aura::Window* new_root) {
  if (GetWidget() && GetWidget()->GetCompositor() &&
      GetWidget()->GetCompositor()->HasObserver(this)) {
    GetWidget()->GetCompositor()->RemoveObserver(this);
  }
}

void FPSGraphPageView::UpdateStats(const gfx::PresentationFeedback& feedback) {
  constexpr base::TimeDelta kOneSec = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta k500ms = base::TimeDelta::FromMilliseconds(500);
  if (!feedback.failed())
    presented_times_.push_back(feedback.timestamp);

  const base::TimeTicks deadline_1s = feedback.timestamp - kOneSec;
  while (!presented_times_.empty() && presented_times_.front() <= deadline_1s)
    presented_times_.pop_front();

  const base::TimeTicks deadline_500ms = feedback.timestamp - k500ms;
  frame_rate_for_last_half_second_ = 0;
  for (auto i = presented_times_.crbegin();
       (i != presented_times_.crend()) && (*i > deadline_500ms); ++i) {
    ++frame_rate_for_last_half_second_;
  }
  frame_rate_for_last_half_second_ *= 2;
}

void FPSGraphPageView::UpdateTopLabel(float refresh_rate) {
  const float refresh_rate_rounded_10 =
      ceilf(unsigned(refresh_rate) / 10.0F) * 10;
  if (grid_->top_label() != refresh_rate_rounded_10) {
    frame_rate_1s_.Reset();
    frame_rate_500ms_.Reset();
    grid_->SetTopLabel(refresh_rate_rounded_10);
    grid_->SetVerticalTicsInterval(kVerticalTicFrames /
                                   refresh_rate_rounded_10);
  }
}

}  // namespace hud_display
}  // namespace ash
