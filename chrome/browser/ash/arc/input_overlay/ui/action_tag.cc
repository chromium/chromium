// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_tag.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"

namespace arc {
namespace input_overlay {
namespace {
constexpr int kIconSize = 20;
constexpr int kCornerRadius = 6;
constexpr SkColor kEditModeBgColor = SK_ColorWHITE;
constexpr SkColor kViewModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
constexpr gfx::Size kImageButtonSize(32, 32);
}  // namespace

class ActionTag::ActionImage : public views::ImageButton {
 public:
  explicit ActionImage(std::string mouse_action)
      : views::ImageButton(), mouse_action_(mouse_action) {
    SetToViewMode();
  }

  ActionImage() : views::ImageButton() { SetToViewMode(); }

  ~ActionImage() override = default;

  void SetDisplayMode(DisplayMode mode) {
    switch (mode) {
      case DisplayMode::kMenu:
      case DisplayMode::kView:
        SetToViewMode();
        break;
      case DisplayMode::kEdit:
        SetToEditMode();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void SetToViewMode() {
    if (mouse_action_.empty())
      return;
    if (mouse_action_ == kPrimaryClick) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
    SetBackground(
        views::CreateRoundedRectBackground(kViewModeBgColor, kCornerRadius));
  }

  void SetToEditMode() {
    if (mouse_action_ == kPrimaryClick) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
    SetBackground(
        views::CreateRoundedRectBackground(kEditModeBgColor, kCornerRadius));
  }

 private:
  std::string mouse_action_;
};

ActionTag::ActionTag() : views::View() {}
ActionTag::~ActionTag() = default;

void ActionTag::SetTextActionTag(const std::string& text) {
  if (image_) {
    RemoveChildViewT(image_);
    image_ = nullptr;
  }

  if (!label_) {
    auto label = std::make_unique<ActionLabel>(base::UTF8ToUTF16(text));
    label->SetPosition(gfx::Point());
    label_ = AddChildView(std::move(label));
  } else {
    label_->SetText(base::UTF8ToUTF16(text));
  }
  label_->SetSize(label_->GetPreferredSize());
}

void ActionTag::SetImageActionTag(const std::string& mouse_action) {
  RemoveAllChildViews();
  label_ = nullptr;
  image_ = nullptr;

  auto image = std::make_unique<ActionImage>(mouse_action);
  image->SetAccessibleName(base::UTF8ToUTF16(image->GetClassName()));
  image->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  image->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  image->SetToViewMode();
  image->SetPosition(gfx::Point());
  image->SetSize(kImageButtonSize);
  image_ = AddChildView(std::move(image));
}

// static
std::unique_ptr<ActionTag> ActionTag::CreateTextActionTag(std::string text) {
  auto tag = std::make_unique<ActionTag>();
  tag->SetTextActionTag(text);
  return tag;
}

// static
std::unique_ptr<ActionTag> ActionTag::CreateImageActionTag(
    std::string mouse_action) {
  DCHECK(mouse_action == kPrimaryClick || mouse_action == kSecondaryClick);
  if (mouse_action != kPrimaryClick && mouse_action != kSecondaryClick)
    return nullptr;
  auto tag = std::make_unique<ActionTag>();
  tag->SetImageActionTag(mouse_action);

  return tag;
}

void ActionTag::SetDisplayMode(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::kMenu:
    case DisplayMode::kView:
      if (label_)
        label_->SetToViewMode();
      if (image_)
        image_->SetToViewMode();
      break;
    case DisplayMode::kEdit:
    case DisplayMode::kEdited:
      if (label_)
        label_->SetToEditMode();
      if (image_)
        image_->SetToEditMode();
      break;
    case DisplayMode::kEditedUnbound:
      if (label_)
        label_->SetToEditedUnBind();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ActionTag::ShowErrorMsg(base::StringPiece error_msg) {
  static_cast<ActionView*>(parent())->ShowErrorMsg(error_msg);
}

void ActionTag::OnKeyBindingChange(ui::DomCode code) {
  DCHECK(parent());
  static_cast<ActionView*>(parent())->OnKeyBindingChange(this, code);
}

gfx::Size ActionTag::CalculatePreferredSize() const {
  DCHECK((label_ && !image_) || (!label_ && image_));
  if (image_)
    return image_->size();
  return label_ ? label_->GetPreferredSize() : gfx::Size();
}

}  // namespace input_overlay
}  // namespace arc
