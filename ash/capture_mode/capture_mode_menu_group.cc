// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_menu_group.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kMenuGroupPadding{8, 0};

constexpr gfx::Insets kMenuHeaderPadding{8, 16};

constexpr gfx::Insets kOptionPadding{8, 52, 8, 16};

constexpr gfx::Insets kMenuItemPadding{10, 52, 10, 16};

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

void SetInkDropForButton(views::Button* button) {
  auto* ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  ink_drop->SetVisibleOpacity(capture_mode::kInkDropVisibleOpacity);
  views::InkDrop::UseInkDropForFloodFillRipple(ink_drop,
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/false);
  ink_drop->SetCreateHighlightCallback(base::BindRepeating(
      [](views::Button* host) {
        const AshColorProvider::RippleAttributes ripple_attributes =
            AshColorProvider::Get()->GetRippleAttributes();
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), ripple_attributes.base_color);
        highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
        return highlight;
      },
      button));
  ink_drop->SetBaseColor(capture_mode::kInkDropBaseColor);
  views::InstallRectHighlightPathGenerator(button);
}

// The header of the menu group, which has an icon and a text label. Not user
// interactable.
class CaptureModeMenuHeader : public views::View {
 public:
  METADATA_HEADER(CaptureModeMenuHeader);

  CaptureModeMenuHeader(const gfx::VectorIcon& icon,
                        std::u16string header_laber)
      : icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(header_laber)))) {
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

// A button which has a text label. Its behavior on click can be customized.
// For selecting folder, a folder window will be opened on click.
class CaptureModeMenuItem : public views::Button {
 public:
  METADATA_HEADER(CaptureModeMenuItem);

  CaptureModeMenuItem(views::Button::PressedCallback callback,
                      std::u16string item_label)
      : views::Button(callback),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(item_label)))) {
    SetBorder(views::CreateEmptyBorder(kMenuItemPadding));
    ConfigLabelView(label_view_);
    CreateAndInitBoxLayoutForView(this);
    SetInkDropForButton(this);
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

// A button which represents an option of the menu group. It has a text label
// and a checked icon. The checked icon will be shown on button click and any
// other option's checked icon will be set to invisible in the meanwhile. One
// and only one checked icon is visible in the menu group.
class CaptureModeOption : public views::Button {
 public:
  METADATA_HEADER(CaptureModeOption);

  CaptureModeOption(views::Button::PressedCallback callback,
                    std::u16string option_label,
                    int option_id,
                    bool checked)
      : views::Button(callback),
        label_view_(AddChildView(
            std::make_unique<views::Label>(std::move(option_label)))),
        checked_icon_view_(AddChildView(std::make_unique<views::ImageView>())),
        id_(option_id) {
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
    SetInkDropForButton(this);
  }

  CaptureModeOption(const CaptureModeOption&) = delete;
  CaptureModeOption& operator=(const CaptureModeOption&) = delete;
  ~CaptureModeOption() override = default;

  int id() const { return id_; }

  void SetOptionChecked(bool checked) {
    checked_icon_view_->SetVisible(checked);
  }

  bool IsOptionChecked() { return checked_icon_view_->GetVisible(); }

 private:
  views::Label* label_view_;
  views::ImageView* checked_icon_view_;
  const int id_;
};

BEGIN_METADATA(CaptureModeOption, views::Button)
END_METADATA

CaptureModeMenuGroup::CaptureModeMenuGroup(Delegate* delegate,
                                           const gfx::VectorIcon& header_icon,
                                           std::u16string header_label)
    : delegate_(delegate) {
  AddChildView(std::make_unique<CaptureModeMenuHeader>(
      header_icon, std::move(header_label)));
  options_container_ = AddChildView(std::make_unique<views::View>());
  options_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMenuGroupPadding,
      kSpaceBetweenMenuItem));
}

CaptureModeMenuGroup::~CaptureModeMenuGroup() = default;

void CaptureModeMenuGroup::AddOption(std::u16string option_label,
                                     int option_id) {
  options_.push_back(
      options_container_->AddChildView(std::make_unique<CaptureModeOption>(
          base::BindRepeating(&CaptureModeMenuGroup::HandleOptionClick,
                              base::Unretained(this), option_id),
          std::move(option_label), option_id,
          /*checked=*/delegate_->IsOptionChecked(option_id))));
}

void CaptureModeMenuGroup::AddMenuItem(views::Button::PressedCallback callback,
                                       std::u16string item_label) {
  views::View::AddChildView(
      std::make_unique<CaptureModeMenuItem>(callback, std::move(item_label)));
}

views::View* CaptureModeMenuGroup::GetOptionForTesting(int option_id) {
  for (auto* option : options_) {
    if (option->id() == option_id)
      return option;
  }
  return nullptr;
}

bool CaptureModeMenuGroup::IsOptionCheckedForTesting(views::View* option) {
  return static_cast<CaptureModeOption*>(option)->IsOptionChecked();
}

void CaptureModeMenuGroup::HandleOptionClick(int option_id) {
  for (auto* option : options_)
    option->SetOptionChecked(option_id == option->id());
  delegate_->OnOptionSelected(option_id);
}

BEGIN_METADATA(CaptureModeMenuGroup, views::View)
END_METADATA
}  // namespace ash
