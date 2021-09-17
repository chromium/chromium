// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_menu_group.h"

#include <memory>
#include <vector>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kMenuGroupPadding{8, 16};

constexpr gfx::Insets kMenuHeaderPadding{8, 0};

constexpr gfx::Insets kOptionPadding{8, 36, 8, 0};

constexpr gfx::Insets kMenuItemPadding{10, 36, 10, 0};

constexpr int kSpaceBetweenMenuItem = 0;

constexpr gfx::Size kIconSize{20, 20};

void ConfigLabelView(views::Label* label_view) {
  label_view->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  label_view->SetBackgroundColor(SK_ColorTRANSPARENT);
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
}

views::BoxLayout* CreateAndInitBoxLayoutForView(views::View* view) {
  auto* box_layout = view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      capture_mode::kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  return box_layout;
}

// The header of the menu group, which has an icon and a text label. Not user
// interactable.
class CaptureModeMenuHeader : public views::View {
 public:
  METADATA_HEADER(CaptureModeMenuHeader);

  CaptureModeMenuHeader(const gfx::VectorIcon& icon, int string_id)
      : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        label_view_(AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(string_id)))) {
    icon_view_->SetImageSize(kIconSize);
    icon_view_->SetPreferredSize(kIconSize);
    icon_view_->SetImage(gfx::CreateVectorIcon(
        icon, AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kButtonIconColor)));

    SetBorder(views::CreateEmptyBorder(kMenuHeaderPadding));
    ConfigLabelView(label_view_);
    CreateAndInitBoxLayoutForView(this);
  }

  CaptureModeMenuHeader(const CaptureModeMenuHeader&) = delete;
  CaptureModeMenuHeader& operator=(const CaptureModeMenuHeader&) = delete;
  ~CaptureModeMenuHeader() override = default;

 private:
  views::ImageView* icon_view_;
  views::Label* label_view_;
};

BEGIN_METADATA(CaptureModeMenuHeader, views::View)
END_METADATA

// A button which represents an option of the menu group. It has a text label
// and a checked icon. The checked icon will be shown on button click and any
// other option's checked icon will be set to invisible in the meanwhile. One
// and only one checked icon is visible in the menu group.
class CaptureModeOption : public views::Button {
 public:
  METADATA_HEADER(CaptureModeOption);

  CaptureModeOption(views::Button::PressedCallback callback,
                    int string_id,
                    bool checked)
      : views::Button(callback),
        label_view_(AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(string_id)))),
        checked_icon_view_(AddChildView(std::make_unique<views::ImageView>())) {
    checked_icon_view_->SetImageSize(kIconSize);
    checked_icon_view_->SetPreferredSize(kIconSize);
    checked_icon_view_->SetImage(gfx::CreateVectorIcon(
        kHollowCheckCircleIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kButtonLabelColorBlue)));
    checked_icon_view_->SetVisible(checked);

    SetBorder(views::CreateEmptyBorder(kOptionPadding));
    ConfigLabelView(label_view_);
    auto* box_layout = CreateAndInitBoxLayoutForView(this);
    box_layout->SetFlexForView(label_view_, 1);
  }

  CaptureModeOption(const CaptureModeOption&) = delete;
  CaptureModeOption& operator=(const CaptureModeOption&) = delete;
  ~CaptureModeOption() override = default;

 private:
  views::Label* label_view_;
  views::ImageView* checked_icon_view_;
};

BEGIN_METADATA(CaptureModeOption, views::Button)
END_METADATA

// A button which has a text label. Its behavior on click can be customized.
// For selecting folder, a folder window will be opened on click.
class CaptureModeMenuItem : public views::Button {
 public:
  METADATA_HEADER(CaptureModeMenuItem);

  CaptureModeMenuItem(views::Button::PressedCallback callback, int string_id)
      : views::Button(callback),
        label_view_(AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(string_id)))) {
    SetBorder(views::CreateEmptyBorder(kMenuItemPadding));
    ConfigLabelView(label_view_);
    CreateAndInitBoxLayoutForView(this);
  }

  CaptureModeMenuItem(const CaptureModeMenuItem&) = delete;
  CaptureModeMenuItem& operator=(const CaptureModeMenuItem&) = delete;
  ~CaptureModeMenuItem() override = default;

 private:
  views::Label* label_view_;
};

BEGIN_METADATA(CaptureModeMenuItem, views::Button)
END_METADATA

}  // namespace

CaptureModeMenuGroup::CaptureModeMenuGroup(const gfx::VectorIcon& header_icon,
                                           int header_label_string_id) {
  AddChildView(std::make_unique<CaptureModeMenuHeader>(header_icon,
                                                       header_label_string_id));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMenuGroupPadding,
      kSpaceBetweenMenuItem));
}

CaptureModeMenuGroup::~CaptureModeMenuGroup() = default;

void CaptureModeMenuGroup::AddOption(views::Button::PressedCallback callback,
                                     int string_id,
                                     bool checked) {
  AddChildView(
      std::make_unique<CaptureModeOption>(callback, string_id, checked));
}

void CaptureModeMenuGroup::AddMenuItem(views::Button::PressedCallback callback,
                                       int string_id) {
  views::View::AddChildView(
      std::make_unique<CaptureModeMenuItem>(callback, string_id));
}

BEGIN_METADATA(CaptureModeMenuGroup, views::View)
END_METADATA
}  // namespace ash
