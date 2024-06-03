// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_rect_cutout_path_builder.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 4;
constexpr size_t kGridViewRowGroupSize = 1;
constexpr size_t kGridViewColGroupSize = 1;

struct CutoutEntry {
  std::u16string name;
  SkColor color;
  std::vector<RoundedRectCutoutPathBuilder::Corner> corners;
  gfx::Size cutout_size;
  std::optional<int> radius;
  std::optional<int> outer_radius;
  std::optional<int> inner_radius;
};

class CutoutsGridView : public SystemUIComponentsGridView {
 public:
  CutoutsGridView()
      : SystemUIComponentsGridView(kGridViewRowNum,
                                   kGridViewColNum,
                                   kGridViewRowGroupSize,
                                   kGridViewColGroupSize) {}
  CutoutsGridView(const CutoutsGridView&) = delete;
  CutoutsGridView& operator=(const CutoutsGridView&) = delete;

  ~CutoutsGridView() override = default;

  void AddCutoutSample(const CutoutEntry& entry) {
    auto view = std::make_unique<views::View>();
    view->SetBackground(views::CreateSolidBackground(entry.color));
    view->SetPreferredSize(std::make_optional<gfx::Size>(200, 150));
    auto builder =
        RoundedRectCutoutPathBuilder(gfx::SizeF(view->GetPreferredSize()));
    if (!entry.corners.empty()) {
      for (const auto& corner : entry.corners) {
        builder.AddCutout(corner, gfx::SizeF(entry.cutout_size));
      }
    }
    if (entry.radius.has_value()) {
      builder.CornerRadius(entry.radius.value());
    }

    if (entry.outer_radius.has_value()) {
      builder.CutoutOuterCornerRadius(entry.outer_radius.value());
    }

    if (entry.inner_radius.has_value()) {
      builder.CutoutInnerCornerRadius(entry.inner_radius.value());
    }

    view->SetClipPath(builder.Build());
    AddInstance(entry.name, std::move(view));
  }
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateCutoutsGridView() {
  const std::array<CutoutEntry, kGridViewRowNum * kGridViewColNum> entries = {{
      // Row 1
      {u"TopLeft",
       SK_ColorRED,
       {RoundedRectCutoutPathBuilder::Corner::kUpperLeft},
       gfx::Size(40, 30)},
      {u"TopRight",
       SK_ColorCYAN,
       {RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       gfx::Size(30, 50)},
      {u"BottomLeft",
       SK_ColorGREEN,
       {RoundedRectCutoutPathBuilder::Corner::kLowerLeft},
       gfx::Size(40, 40)},
      {u"BottomRight",
       SK_ColorMAGENTA,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight},
       gfx::Size(30, 30)},
      // Row 2
      {u"TopLeft 4px corner",
       SK_ColorRED,
       {RoundedRectCutoutPathBuilder::Corner::kUpperLeft},
       gfx::Size(80, 30),
       4},
      {u"TopRight 8px corner",
       SK_ColorCYAN,
       {RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       gfx::Size(30, 50),
       8},
      {u"BottomLeft 12px corner",
       SK_ColorGREEN,
       {RoundedRectCutoutPathBuilder::Corner::kLowerLeft},
       gfx::Size(40, 40),
       12},
      {u"BottomRight 20px corner",
       SK_ColorMAGENTA,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight},
       gfx::Size(30, 30),
       20},
      // Row 3
      {u"No cutout", SK_ColorBLACK, {}, gfx::Size()},
      {u"4px corner. 6px small, 12px inner",
       SK_ColorBLUE,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight},
       gfx::Size(40, 40),
       4,
       6,
       12},
      {u"60px corner, 12px small, 4px inner",
       SK_ColorMAGENTA,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight},
       gfx::Size(60, 60),
       60,
       12,
       4},
      {u"Everything 6px",
       SK_ColorRED,
       {RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       gfx::Size(20, 20),
       6,
       6,
       6},
      // Row 4
      {u"Lower Cutouts",
       SK_ColorBLUE,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
        RoundedRectCutoutPathBuilder::Corner::kLowerLeft},
       gfx::Size(40, 40),
       8,
       16,
       12},
      {u"Across",
       SK_ColorMAGENTA,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
        RoundedRectCutoutPathBuilder::Corner::kUpperLeft},
       gfx::Size(40, 40),
       8,
       16,
       12},
      {u"3 cutouts",
       SK_ColorRED,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
        RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
        RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       gfx::Size(40, 40),
       8,
       16,
       12},
      {u"All 4 Cutouts",
       SK_ColorGREEN,
       {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
        RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
        RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
        RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       gfx::Size(88, 60),
       16,
       12,
       20},
  }};

  auto grid_view = std::make_unique<CutoutsGridView>();

  for (const auto& entry : entries) {
    grid_view->AddCutoutSample(entry);
  }

  return grid_view;
}

}  // namespace ash
