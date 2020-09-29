// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/cpu_graph_page_view.h"

#include <algorithm>
#include <numeric>

#include "ash/hud_display/hud_constants.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace hud_display {

////////////////////////////////////////////////////////////////////////////////
// CpuGraphPageView, public:

BEGIN_METADATA(CpuGraphPageView, GraphPageViewBase)
END_METADATA

CpuGraphPageView::CpuGraphPageView(const base::TimeDelta refresh_interval)
    : cpu_other_(Graph::Baseline::BASELINE_BOTTOM,
                 Graph::Fill::SOLID,
                 SkColorSetA(SK_ColorMAGENTA, kHUDAlpha)),
      cpu_system_(Graph::Baseline::BASELINE_BOTTOM,
                  Graph::Fill::SOLID,
                  SkColorSetA(SK_ColorRED, kHUDAlpha)),
      cpu_user_(Graph::Baseline::BASELINE_BOTTOM,
                Graph::Fill::SOLID,
                SkColorSetA(SK_ColorBLUE, kHUDAlpha)),
      cpu_idle_(Graph::Baseline::BASELINE_BOTTOM,
                Graph::Fill::SOLID,
                SkColorSetA(SK_ColorDKGRAY, kHUDAlpha)) {
  const int data_width = cpu_other_.GetDataBufferSize();
  // -XX seconds on the left, 100% top, 0 seconds on the right, 0% on the
  // bottom. Seconds and Gigabytes are dimentions. Number of data points is
  // cpu_other_.GetDataBufferSize(), horizontal grid ticks are drawn every 10
  // seconds.
  CreateGrid(
      /*left=*/static_cast<int>(-data_width * refresh_interval.InSecondsF()),
      /*top=*/100, /*right=*/0, /*bottom=*/0, base::ASCIIToUTF16("s"),
      base::ASCIIToUTF16("%"), data_width, 10 / refresh_interval.InSecondsF());

  Legend::Formatter formatter = base::BindRepeating([](float value) {
    return base::ASCIIToUTF16(base::StringPrintf(
        "%d %%", std::min(100, std::max(0, (int)(value * 100)))));
  });

  const std::vector<Legend::Entry> legend(
      {{cpu_idle_, base::ASCIIToUTF16("Idle"),
        base::ASCIIToUTF16("Total amount of CPU time spent\nin idle mode."),
        formatter},
       {cpu_user_, base::ASCIIToUTF16("User"),
        base::ASCIIToUTF16(
            "Total amount of CPU time spent\n running user processes."),
        formatter},
       {cpu_system_, base::ASCIIToUTF16("System"),
        base::ASCIIToUTF16(
            "Total amount of CPU time spent\nrunning system processes."),
        formatter},
       {cpu_other_, base::ASCIIToUTF16("Other"),
        base::ASCIIToUTF16(
            "Total amount of CPU time spent\nrunning other tasks.\nThis "
            "includes IO wait, IRQ, guest OS, etc."),
        formatter}});
  CreateLegend(legend);
}

CpuGraphPageView::~CpuGraphPageView() = default;

////////////////////////////////////////////////////////////////////////////////

void CpuGraphPageView::OnPaint(gfx::Canvas* canvas) {
  // TODO: Should probably update last graph point more often than shift graph.

  // Layout graphs.
  gfx::Rect rect = GetContentsBounds();
  // Adjust to grid width.
  rect.Inset(kGridLineWidth, kGridLineWidth);
  cpu_other_.Layout(rect, nullptr /* base*/);
  cpu_system_.Layout(rect, &cpu_other_);
  cpu_user_.Layout(rect, &cpu_system_);
  cpu_idle_.Layout(rect, &cpu_user_);

  // Paint damaged area now that all parameters have been determined.
  cpu_other_.Draw(canvas);
  cpu_system_.Draw(canvas);
  cpu_user_.Draw(canvas);
  cpu_idle_.Draw(canvas);
}

void CpuGraphPageView::UpdateData(const DataSource::Snapshot& snapshot) {
  // TODO: Should probably update last graph point more often than shift graph.
  const float total = snapshot.cpu_idle_part + snapshot.cpu_user_part +
                      snapshot.cpu_system_part + snapshot.cpu_other_part;
  // Nothing to do if data is not available yet (sum < 1%).
  if (total < 0.01)
    return;

  // Assume total already equals 1, no need to re-weight.

  // Update graph data.
  // unscaled values for CPU are the same. Formatter will display it as %.
  cpu_other_.AddValue(snapshot.cpu_other_part, snapshot.cpu_other_part);
  cpu_system_.AddValue(snapshot.cpu_system_part, snapshot.cpu_system_part);
  cpu_user_.AddValue(snapshot.cpu_user_part, snapshot.cpu_user_part);
  cpu_idle_.AddValue(snapshot.cpu_idle_part, snapshot.cpu_idle_part);

  RefreshLegendValues();
}

}  // namespace hud_display
}  // namespace ash
