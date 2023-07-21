// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/message_view.h"

#include "ash/ambient/util/ambient_util.h"
#include "ash/public/cpp/view_shadow.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

namespace arc::input_overlay {

namespace {
// UI specs.
constexpr char kFontStyle[] = "Roboto";
constexpr int kFontSize = 13;
constexpr int kLineHeight = 20;
constexpr int kCornerRadius = 12;
constexpr int kShadowElevation = 2;
constexpr int kSideInset = 24;
constexpr int kTopMargin = 24;
constexpr int kMinHeight = 72;
constexpr int kMaxTextWidth = 256;
constexpr SkColor kTextColor = gfx::kGoogleGrey200;
constexpr SkColor kBackgroundColor = gfx::kGoogleGrey900;
constexpr SkColor kForegroundColor = SkColorSetA(SK_ColorWHITE, 0x0F);

// About Icon.
constexpr int kIconSize = 32;
constexpr int kImageLabelSpace = 24;
constexpr SkColor kInfoIconColor = gfx::kGoogleBlue300;
constexpr SkColor kErrorIconColor = gfx::kGoogleRed300;
}  // namespace

// static
MessageView* MessageView::Show(DisplayOverlayController* controller,
                               views::View* parent,
                               const base::StringPiece& message,
                               MessageType message_type) {
  auto* view_ptr = parent->AddChildView(std::make_unique<MessageView>(
      controller, parent->size(), message, message_type));
  view_ptr->AddShadow();
  return view_ptr;
}

MessageView::MessageView(DisplayOverlayController* controller,
                         const gfx::Size& parent_size,
                         const base::StringPiece& message,
                         MessageType message_type)
    : views::LabelButton(), display_overlay_controller_(controller) {
  DCHECK(display_overlay_controller_);
  if (display_overlay_controller_) {
    display_overlay_controller_->RemoveEditMessage();
  }
  SetBackground(views::CreateRoundedRectBackground(
      color_utils::GetResultingPaintColor(kForegroundColor, kBackgroundColor),
      kCornerRadius));
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kSideInset)));
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetText(base::UTF8ToUTF16(message));
  SetEnabledTextColors(kTextColor);
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::NORMAL));
  label()->SetLineHeight(kLineHeight);
  label()->SetMultiLine(true);

  SetImageLabelSpacing(kImageLabelSpace);
  image()->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  switch (message_type) {
    case MessageType::kInfo:
      SetImage(views::Button::STATE_NORMAL,
               gfx::CreateVectorIcon(gfx::IconDescription(
                   vector_icons::kInfoOutlineIcon, kIconSize, kInfoIconColor)));
      break;
    case MessageType::kError:
      SetImage(
          views::Button::STATE_NORMAL,
          gfx::CreateVectorIcon(gfx::IconDescription(
              vector_icons::kErrorOutlineIcon, kIconSize, kErrorIconColor)));
      break;
    case MessageType::kInfoLabelFocus:
      SetImage(views::Button::STATE_NORMAL,
               gfx::CreateVectorIcon(gfx::IconDescription(
                   vector_icons::kKeyboardIcon, kIconSize, kInfoIconColor)));
      break;
    default:
      NOTREACHED();
      break;
  }

  auto preferred_size =
      gfx::Size(kMaxTextWidth + kIconSize + kImageLabelSpace + 2 * kSideInset,
                kMinHeight);
  preferred_size.SetToMin(parent_size);
  SetSize(preferred_size);
  SetPosition(gfx::Point(
      std::max(0, (parent_size.width() - preferred_size.width()) / 2),
      kTopMargin));
}

MessageView::~MessageView() = default;

void MessageView::AddShadow() {
  view_shadow_ = std::make_unique<ash::ViewShadow>(this, kShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kCornerRadius);
}

}  // namespace arc::input_overlay
