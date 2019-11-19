// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_list_view.h"

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_view.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace keyboard_shortcut_viewer {

namespace {

// A horizontal line to separate the KeyboardShortcutItemView.
class HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width)
      : preferred_width_(preferred_width) {}

  ~HorizontalSeparator() override = default;

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    constexpr int kSeparatorThickness = 1;
    return gfx::Size(preferred_width_, kSeparatorThickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect contents_bounds(GetContentsBounds());
    constexpr SkColor kSeparatorColor = SkColorSetARGB(0x0F, 0x00, 0x00, 0x00);
    canvas->FillRect(contents_bounds, kSeparatorColor);
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

}  // namespace

KeyboardShortcutItemListView::KeyboardShortcutItemListView() {
  // The padding values are by design. Lesser padding between list view and the
  // side tab than the padding between list view and the border of the window.
  constexpr int kLeftPadding = 16;
  constexpr int kRightPadding = 32;
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));
  SetBorder(
      views::CreateEmptyBorder(gfx::Insets(0, kLeftPadding, 0, kRightPadding)));
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kList);
}

void KeyboardShortcutItemListView::AddCategoryLabel(
    const base::string16& text) {
  constexpr int kLabelTopPadding = 44;
  constexpr int kLabelBottomPadding = 20;
  constexpr SkColor kLabelColor = SkColorSetARGB(0xFF, 0x42, 0x85, 0xF4);

  views::Label* category_label = new views::Label(text);
  category_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  category_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kLabelTopPadding, 0, kLabelBottomPadding, 0)));
  category_label->SetEnabledColor(kLabelColor);
  constexpr int kLabelFontSizeDelta = 1;
  category_label->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  AddChildView(category_label);
}

void KeyboardShortcutItemListView::AddHorizontalSeparator() {
  AddChildView(new HorizontalSeparator(bounds().width()));
}

}  // namespace keyboard_shortcut_viewer
