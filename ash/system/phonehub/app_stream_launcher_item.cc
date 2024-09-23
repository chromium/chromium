// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_item.h"

#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr double kAlphaValueForInhibitedIconOpacity = 0.38;

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
  METADATA_HEADER(AppNameLabel, views::LabelButton)

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

BEGIN_METADATA(AppNameLabel)
END_METADATA

}  // namespace

AppStreamLauncherItem::AppStreamLauncherItem(
    base::RepeatingClosure callback,
    const phonehub::Notification::AppMetadata& app_metadata) {
  SetPreferredSize(kEcheAppItemSize);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kEcheAppItemSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const bool enabled = app_metadata.app_streamability_status ==
                       phonehub::proto::AppStreamabilityStatus::STREAMABLE;
  gfx::Image image = app_metadata.color_icon;
  if (!enabled) {
    // Fade the image in order to make it look like grayed out.
    image = gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(
        image.AsImageSkia(), kAlphaValueForInhibitedIconOpacity));
  }

  std::u16string accessible_name;
  switch (app_metadata.app_streamability_status) {
    case phonehub::proto::STREAMABLE:
      accessible_name = app_metadata.visible_app_name;
      break;
    case phonehub::proto::BLOCKED_BY_APP:
      accessible_name = l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_STREAM_NOT_SUPPORTED_BY_APP);
      break;
    case phonehub::proto::BLOCK_LISTED:
    default:
      accessible_name =
          l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_STREAM_NOT_SUPPORTED);
      break;
  }
  recent_app_button_ = AddChildView(std::make_unique<PhoneHubRecentAppButton>(
      image, app_metadata.visible_app_name, callback));
  recent_app_button_->GetViewAccessibility().SetName(accessible_name);
  recent_app_button_->SetTooltipText(accessible_name);
  recent_app_button_->SetEnabled(enabled);

  label_ = AddChildView(std::make_unique<AppNameLabel>(
      std::move(callback), app_metadata.visible_app_name));
  label_->SetEnabled(enabled);
  label_->GetViewAccessibility().SetName(accessible_name);
  label_->SetTooltipText(accessible_name);
}

AppStreamLauncherItem::~AppStreamLauncherItem() = default;

bool AppStreamLauncherItem::HasFocus() const {
  return recent_app_button_->HasFocus() || label_->HasFocus();
}

void AppStreamLauncherItem::RequestFocus() {
  recent_app_button_->RequestFocus();
}

views::LabelButton* AppStreamLauncherItem::GetLabelForTest() {
  return label_;
}
PhoneHubRecentAppButton* AppStreamLauncherItem::GetIconForTest() {
  return recent_app_button_;
}

BEGIN_METADATA(AppStreamLauncherItem)
END_METADATA

}  // namespace ash
