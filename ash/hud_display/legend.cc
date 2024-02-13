// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/legend.h"

#include <string_view>

#include "ash/hud_display/graph.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/solid_source_background.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {

namespace {

class LegendEntry : public views::View {
  METADATA_HEADER(LegendEntry, views::View)

 public:
  explicit LegendEntry(const Legend::Entry& data);

  LegendEntry(const LegendEntry&) = delete;
  LegendEntry& operator=(const LegendEntry&) = delete;

  ~LegendEntry() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  void SetValueIndex(size_t index);
  void RefreshValue();

  // This is used by parent to match sizes.
  views::View* value() { return value_; }

 private:
  const SkColor color_;
  const raw_ref<const Graph> graph_;
  size_t value_index_ = 0;
  Legend::Formatter formatter_;
  raw_ptr<views::Label> value_ = nullptr;
};

BEGIN_METADATA(LegendEntry)
END_METADATA

LegendEntry::LegendEntry(const Legend::Entry& data)
    : color_(data.graph->color()),
      graph_(*data.graph),
      formatter_(data.formatter) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // We need to allocate space for the colorpicker. It should be square with
  // edge size matching default views::Label height.  This is not known until
  // layout runs, so just hard code it.
  constexpr int kColorpickerAreaWidth = 20;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kColorpickerAreaWidth, 0, 0)));

  views::Label* label = AddChildView(
      std::make_unique<views::Label>(data.label, views::style::CONTEXT_LABEL));
  label->SetEnabledColor(kHUDDefaultColor);
  if (!data.tooltip.empty())
    label->SetTooltipText(data.tooltip);

  constexpr int kLabelToValueSpece = 5;
  value_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL));
  layout_manager->SetFlexForView(value_, /*flex=*/1);
  value_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
  value_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, kLabelToValueSpece, 0, 0)));
  value_->SetEnabledColor(kHUDDefaultColor);
}

LegendEntry::~LegendEntry() = default;

void LegendEntry::OnPaint(gfx::Canvas* canvas) {
  // Draw 10x10 sold color rectangle in the middle of the left border.
  // (We used border to allocate space for the colorpicker above.)
  constexpr int kBoxSize = 10;
  const gfx::Rect bounds(GetInsets().left(), height());

  constexpr int kBoxBorderWidth = 1;

  gfx::Rect box = bounds;
  box.ClampToCenteredSize(gfx::Size(kBoxSize, kBoxSize));

  const SkRect border =
      SkRect::MakeXYWH(box.x(), box.y(), box.width(), box.height());

  SkPath box_border;
  box_border.addRect(border);

  SkPath box_filled;
  box_filled.addRect(border.makeInset(kBoxBorderWidth, kBoxBorderWidth));

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color_);
  canvas->DrawPath(box_filled, flags);

  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kBoxBorderWidth);
  flags.setColor(kHUDDefaultColor);
  canvas->DrawPath(box_border, flags);

  views::View::OnPaint(canvas);
}

void LegendEntry::SetValueIndex(size_t index) {
  if (index == value_index_)
    return;

  value_index_ = index;
  RefreshValue();
}

void LegendEntry::RefreshValue() {
  if (graph_->IsFilledIndex(value_index_)) {
    value_->SetText(formatter_.Run(graph_->GetUnscaledValueAt(value_index_)));
  } else {
    value_->SetText(std::u16string());
  }
}

}  // namespace

Legend::Entry::Entry(const Graph& graph,
                     std::u16string label,
                     std::u16string tooltip,
                     Formatter formatter)
    : graph(graph), label(label), tooltip(tooltip), formatter(formatter) {}

Legend::Entry::Entry(const Entry&) = default;

Legend::Entry::~Entry() = default;

BEGIN_METADATA(Legend)
END_METADATA

Legend::Legend(const std::vector<Legend::Entry>& contents) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  SetBackground(std::make_unique<SolidSourceBackground>(kHUDLegendBackground,
                                                        /*radius=*/0));

  SetBorder(views::CreateEmptyBorder(kHUDInset));

  for (const auto& entry : contents)
    AddChildView(std::make_unique<LegendEntry>(entry));
}

Legend::~Legend() = default;

void Legend::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  gfx::Size max_size;
  bool updated = false;
  for (views::View* view : children()) {
    if (std::string_view(view->GetClassName()) !=
        std::string_view(LegendEntry::kViewClassName)) {
      continue;
    }

    views::View* value = static_cast<LegendEntry*>(view)->value();
    max_size.SetToMax(value->GetPreferredSize());
    updated |= max_size != value->GetPreferredSize();
  }
  if (updated) {
    for (views::View* view : children()) {
      if (std::string_view(view->GetClassName()) !=
          std::string_view(LegendEntry::kViewClassName)) {
        continue;
      }

      static_cast<LegendEntry*>(view)->value()->SetPreferredSize(max_size);
    }
    LayoutSuperclass<views::View>(this);
  }
}

void Legend::SetValuesIndex(size_t index) {
  for (views::View* view : children()) {
    if (std::string_view(view->GetClassName()) !=
        std::string_view(LegendEntry::kViewClassName)) {
      continue;
    }

    static_cast<LegendEntry*>(view)->SetValueIndex(index);
  }
}

void Legend::RefreshValues() {
  for (views::View* view : children()) {
    if (std::string_view(view->GetClassName()) !=
        std::string_view(LegendEntry::kViewClassName)) {
      continue;
    }

    static_cast<LegendEntry*>(view)->RefreshValue();
  }
}

}  // namespace hud_display
}  // namespace ash
