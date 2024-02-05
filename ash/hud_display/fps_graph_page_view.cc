// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/fps_graph_page_view.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/reference_lines.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {

namespace {
// Draw tick on the vertical axis every 5 frames.
constexpr float kVerticalTickFrames = 5;
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FPSGraphPageView, public:

BEGIN_METADATA(FPSGraphPageView)
END_METADATA

FPSGraphPageView::FPSGraphPageView(const base::TimeDelta refresh_interval)
    : frame_rate_1s_(kHUDGraphWidth,
                     Graph::Baseline::kBaselineBottom,
                     Graph::Fill::kNone,
                     Graph::Style::kSkyline,
                     SkColorSetA(SK_ColorYELLOW, kHUDAlpha)),
      frame_rate_500ms_(kHUDGraphWidth,
                        Graph::Baseline::kBaselineBottom,
                        Graph::Fill::kNone,
                        Graph::Style::kSkyline,
                        SkColorSetA(SK_ColorCYAN, kHUDAlpha)),
      refresh_rate_(kHUDGraphWidth,
                    Graph::Baseline::kBaselineBottom,
                    Graph::Fill::kNone,
                    Graph::Style::kSkyline,
                    kHUDBackground /*not drawn*/) {
  const int data_width = frame_rate_1s_.max_data_points();
  // Verical ticks are drawn every 5 frames (5/60 interval).
  constexpr float vertical_ticks_interval = kVerticalTickFrames / 60.F;
  // max_data_points left label, 60fps  top, 0 seconds on the right, 0fps on the
  // bottom. Seconds and fps are dimensions. Number of data points is
  // data_width, horizontal tick marks are drawn every 10 frames.
  reference_lines_ = CreateReferenceLines(
      /*left=*/data_width,
      /*top=*/60, /*right=*/0, /*bottom=*/0,
      /*x_unit=*/u"frames",
      /*y_unit=*/u"fps",
      /*horizontal_points_number=*/data_width,
      /*horizontal_ticks_interval=*/10, vertical_ticks_interval);

  Legend::Formatter formatter_float = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(base::StringPrintf("%.1f", value));
  });

  Legend::Formatter formatter_int = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(base::StringPrintf("%d", (int)value));
  });

  const std::vector<Legend::Entry> legend(
      {{refresh_rate_, u"Refresh rate", u"Actual display refresh rate.",
        formatter_int},
       {frame_rate_1s_, u"1s FPS",
        u"Number of frames successfully presented per 1 second.",
        formatter_float},
       {frame_rate_500ms_, u".5s FPS",
        u"Number of frames successfully presented per 0.5 second scaled to a "
        u"second.",
        formatter_float}});
  CreateLegend(legend);
}

FPSGraphPageView::~FPSGraphPageView() = default;

////////////////////////////////////////////////////////////////////////////////

void FPSGraphPageView::AddedToWidget() {
  GraphPageViewBase::AddedToWidget();
  GetWidget()->AddObserver(this);
}

void FPSGraphPageView::RemovedFromWidget() {
  GetWidget()->RemoveObserver(this);
  GraphPageViewBase::RemovedFromWidget();
}

void FPSGraphPageView::OnPaint(gfx::Canvas* canvas) {
  // TODO: Should probably update last graph point more often than shift graph.

  // Layout graphs.
  gfx::Rect rect = GetContentsBounds();
  // Adjust bounds to not overlap with bordering reference lines.
  rect.Inset(kHUDGraphReferenceLineWidth);

  frame_rate_500ms_.Layout(rect, /*base=*/nullptr);
  frame_rate_1s_.Layout(rect, /*base=*/nullptr);

  frame_rate_500ms_.Draw(canvas);
  frame_rate_1s_.Draw(canvas);
  // Refresh rate graph is not drawn, it's just used in Legend display and
  // reference line calculations.
}

void FPSGraphPageView::OnDidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  UpdateStats(feedback);

  float frame_rate_1s = frame_rate_for_last_second();
  float frame_rate_500ms = frame_rate_for_last_half_second();

  float refresh_rate = GetWidget()->GetCompositor()->refresh_rate();

  UpdateTopLabel(refresh_rate);
  frame_rate_1s_.AddValue(frame_rate_1s / reference_lines_->top_label(),
                          frame_rate_1s);

  frame_rate_500ms_.AddValue(frame_rate_500ms / reference_lines_->top_label(),
                             frame_rate_500ms);

  const float max_refresh_rate =
      std::max(refresh_rate, refresh_rate_.GetUnscaledValueAt(0));
  refresh_rate_.AddValue(max_refresh_rate / reference_lines_->top_label(),
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

void FPSGraphPageView::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, GetWidget());
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
  constexpr base::TimeDelta kOneSec = base::Seconds(1);
  constexpr base::TimeDelta k500ms = base::Milliseconds(500);
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
      ceilf(unsigned(refresh_rate) * 0.1F) * 10.F;
  if (reference_lines_->top_label() != refresh_rate_rounded_10) {
    frame_rate_1s_.Reset();
    frame_rate_500ms_.Reset();
    reference_lines_->SetTopLabel(refresh_rate_rounded_10);
    reference_lines_->SetVerticalTicksInterval(kVerticalTickFrames /
                                               refresh_rate_rounded_10);
  }
}

}  // namespace hud_display
}  // namespace ash
