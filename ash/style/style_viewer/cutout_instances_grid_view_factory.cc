// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/rounded_rect_cutout_path_builder.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr size_t kGridViewRowNum = 5;
constexpr size_t kGridViewColNum = 4;
constexpr size_t kGridViewRowGroupSize = 1;
constexpr size_t kGridViewColGroupSize = 1;

// Create multiple cutouts with the same size.
struct CutoutsSpec {
  gfx::Size cutout_size;
  std::vector<RoundedRectCutoutPathBuilder::Corner> corners;
};

// Specify a size and location for a cutout.
struct CutoutSpec {
  gfx::Size size;
  RoundedRectCutoutPathBuilder::Corner corner;
};

struct CutoutEntry {
  std::u16string name;
  SkColor color;
  absl::variant<std::vector<CutoutSpec>, CutoutsSpec> cutouts;
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
    if (absl::holds_alternative<std::vector<CutoutSpec>>(entry.cutouts)) {
      const auto& cutouts = absl::get<std::vector<CutoutSpec>>(entry.cutouts);
      if (!cutouts.empty()) {
        for (const auto& spec : cutouts) {
          builder.AddCutout(spec.corner, gfx::SizeF(spec.size));
        }
      }
    } else {
      const CutoutsSpec& cutouts = absl::get<CutoutsSpec>(entry.cutouts);
      if (!cutouts.corners.empty()) {
        for (const auto& corner : cutouts.corners) {
          builder.AddCutout(corner, gfx::SizeF(cutouts.cutout_size));
        }
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

std::vector<CutoutSpec> MakeSpecs(gfx::Size size,
                                  RoundedRectCutoutPathBuilder::Corner corner) {
  std::vector<CutoutSpec> specs;
  specs.emplace_back(size, corner);
  return specs;
}

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateCutoutsGridView() {
  const std::array<CutoutEntry, kGridViewRowNum * kGridViewColNum> entries = {{
      // Row 1
      {u"TopLeft", SK_ColorRED,
       CutoutsSpec{
           gfx::Size(40, 30),
           {RoundedRectCutoutPathBuilder::Corner::kUpperLeft},
       }},
      {u"TopRight", SK_ColorCYAN,
       CutoutsSpec{
           gfx::Size(30, 50),
           {RoundedRectCutoutPathBuilder::Corner::kUpperRight},
       }},
      {u"BottomLeft", SK_ColorGREEN,
       CutoutsSpec{
           gfx::Size(40, 40),
           {RoundedRectCutoutPathBuilder::Corner::kLowerLeft},
       }},
      {u"BottomRight", SK_ColorMAGENTA,
       CutoutsSpec{gfx::Size(30, 30),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight}}},
      // Row 2
      {u"TopLeft 4px corner", SK_ColorRED,
       CutoutsSpec{gfx::Size(80, 30),
                   {RoundedRectCutoutPathBuilder::Corner::kUpperLeft}},
       4},
      {u"TopRight 8px corner", SK_ColorCYAN,
       CutoutsSpec{gfx::Size(30, 50),
                   {RoundedRectCutoutPathBuilder::Corner::kUpperRight}},
       8},
      {u"BottomLeft 12px corner", SK_ColorGREEN,
       CutoutsSpec{gfx::Size(40, 40),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerLeft}},
       12},
      {u"BottomRight 20px corner", SK_ColorMAGENTA,
       CutoutsSpec{gfx::Size(30, 30),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight}},
       20},
      // Row 3
      {u"No cutout", SK_ColorBLACK, CutoutsSpec{gfx::Size(), {}}},
      {u"4px corner. 6px small, 12px inner", SK_ColorBLUE,
       CutoutsSpec{gfx::Size(40, 40),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight}},
       4, 6, 12},
      {u"60px corner, 12px small, 4px inner", SK_ColorMAGENTA,
       CutoutsSpec{gfx::Size(60, 60),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight}},
       60, 12, 4},
      {u"Everything 6px", SK_ColorRED,
       CutoutsSpec{gfx::Size(20, 20),
                   {RoundedRectCutoutPathBuilder::Corner::kUpperRight}},
       6, 6, 6},
      // Row 4
      {u"Lower Cutouts", SK_ColorBLUE,
       CutoutsSpec{gfx::Size(40, 40),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                    RoundedRectCutoutPathBuilder::Corner::kLowerLeft}},
       8, 16, 12},
      {u"Across", SK_ColorMAGENTA,
       CutoutsSpec{gfx::Size(40, 40),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                    RoundedRectCutoutPathBuilder::Corner::kUpperLeft}},
       8, 16, 12},
      {u"3 cutouts", SK_ColorRED,
       CutoutsSpec{gfx::Size(40, 40),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                    RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
                    RoundedRectCutoutPathBuilder::Corner::kUpperRight}},
       8, 16, 12},
      {u"All 4 Cutouts", SK_ColorGREEN,
       CutoutsSpec{gfx::Size(88, 60),
                   {RoundedRectCutoutPathBuilder::Corner::kLowerRight,
                    RoundedRectCutoutPathBuilder::Corner::kLowerLeft,
                    RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                    RoundedRectCutoutPathBuilder::Corner::kUpperRight}},
       16, 12, 20},
      // Row 5
      {.name = u"Very large cutout",
       .color = SK_ColorMAGENTA,
       .cutouts = MakeSpecs(gfx::Size(120, 80),
                            RoundedRectCutoutPathBuilder::Corner::kLowerLeft),
       .radius = 12,
       .outer_radius = 12,
       .inner_radius = 12},
      {u"One small one large (across)", SK_ColorBLACK,
       std::vector<CutoutSpec>{
           {gfx::Size(120, 80),
            RoundedRectCutoutPathBuilder::Corner::kLowerRight},
           {gfx::Size(35, 35),
            RoundedRectCutoutPathBuilder::Corner::kUpperLeft}},
       4, 4, 4},
      {u"One small one large (vertical)", SK_ColorBLUE,
       std::vector<CutoutSpec>{
           {gfx::Size(120, 80),
            RoundedRectCutoutPathBuilder::Corner::kUpperRight},
           {gfx::Size(35, 35),
            RoundedRectCutoutPathBuilder::Corner::kLowerRight}},
       12, 12, 12},
      {u"One small one large (horizontal)", SK_ColorGREEN,
       std::vector<CutoutSpec>{
           {gfx::Size(120, 80),
            RoundedRectCutoutPathBuilder::Corner::kUpperLeft},
           {gfx::Size(35, 35),
            RoundedRectCutoutPathBuilder::Corner::kUpperRight}},
       8, 8, 8},
  }};

  auto grid_view = std::make_unique<CutoutsGridView>();

  for (const auto& entry : entries) {
    grid_view->AddCutoutSample(entry);
  }

  return grid_view;
}

}  // namespace ash
