// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_item.h"

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kEcheAppItemWidth = 50;
constexpr int kEcheAppItemHeight = 60;
constexpr gfx::Size kEcheAppItemSize(kEcheAppItemWidth, kEcheAppItemHeight);
constexpr int kEcheAppItemSpacing = 4;
constexpr int kEcheAppNameLabelLineHeight = 14;
constexpr int kEcheAppNameLabelFontSize = 11;

void ConfigureLabel(views::Label* label, int line_height, int font_size) {
  label->SetLineHeight(line_height);
  label->SetTruncateLength(kEcheAppItemWidth);

  gfx::Font default_font;
  gfx::Font label_font =
      default_font.Derive(font_size - default_font.GetFontSize(),
                          gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(label_font);
  label->SetFontList(font_list);
}

class AppNameLabel : public views::LabelButton {
 public:
  explicit AppNameLabel(PressedCallback callback = PressedCallback(),
                        const std::u16string& text = std::u16string())
      : LabelButton(std::move(callback), text) {
    ConfigureLabel(label(), kEcheAppNameLabelLineHeight,
                   kEcheAppNameLabelFontSize);
    SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  ~AppNameLabel() override = default;
  AppNameLabel(AppNameLabel&) = delete;
  AppNameLabel operator=(AppNameLabel&) = delete;
};

}  // namespace

AppStreamLauncherItem::AppStreamLauncherItem(
    views::ImageButton::PressedCallback callback,
    const phonehub::Notification::AppMetadata& app_metadata) {
  SetPreferredSize(kEcheAppItemSize);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kEcheAppItemSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  recent_app_button_ = AddChildView(std::make_unique<PhoneHubRecentAppButton>(
      app_metadata.icon, app_metadata.visible_app_name, callback));

  label_ = AddChildView(
      std::make_unique<AppNameLabel>(callback, app_metadata.visible_app_name));
}

AppStreamLauncherItem::~AppStreamLauncherItem() = default;

bool AppStreamLauncherItem::HasFocus() const {
  return recent_app_button_->HasFocus() || label_->HasFocus();
}

void AppStreamLauncherItem::RequestFocus() {
  recent_app_button_->RequestFocus();
}

const char* AppStreamLauncherItem::GetClassName() const {
  return "AppStreamLauncherItem";
}

views::LabelButton* AppStreamLauncherItem::GetLabelForTest() {
  return label_;
}
PhoneHubRecentAppButton* AppStreamLauncherItem::GetIconForTest() {
  return recent_app_button_;
}

}  // namespace ash
