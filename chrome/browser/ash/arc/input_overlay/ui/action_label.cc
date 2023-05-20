// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <string.h>
#include <set>

#include "ash/style/style_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc::input_overlay {
namespace {

constexpr char kFontStyle[] = "Google Sans";
constexpr int kIconSize = 20;

// About colors.
constexpr SkColor kBackgroundColorDefault = SK_ColorWHITE;
constexpr SkColor kTextColorDefault = gfx::kGoogleGrey900;
constexpr SkColor kEditedUnboundBgColor = gfx::kGoogleRed300;
constexpr SkColor kEditInactiveTextColor = SK_ColorWHITE;

// UI specs - AlphaV2.
constexpr gfx::Size kLabelSize(22, 22);
constexpr int kCornerRadius = 4;
constexpr int kFontSize = 14;
constexpr int kSideInset = 4;
// For ActionMove.
constexpr int kCrossPadding =
    9;  // 4 + 4(kCrossOutsideStrokeThickness) + 1(kCrossInsideStrokeThickness)
// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -5;
// Thickness of focus ring.
constexpr float kHaloThickness = 3;

// TODO(b/241966781): remove this and replace it with image asset.
constexpr char16_t kMouseCursorLock[] = u"mouse cursor lock (esc)";

bool IsLeft(TapLabelPosition position) {
  return position == TapLabelPosition::kTopLeft ||
         position == TapLabelPosition::kBottomLeft;
}

bool IsRight(TapLabelPosition position) {
  return !IsLeft(position) && position != TapLabelPosition::kNone;
}

bool IsTop(TapLabelPosition position) {
  return position == TapLabelPosition::kTopLeft ||
         position == TapLabelPosition::kTopRight;
}

bool IsBottom(TapLabelPosition position) {
  return !IsTop(position) && position != TapLabelPosition::kNone;
}

class ActionLabelTap : public ActionLabel {
 public:
  ActionLabelTap(MouseAction mouse_action, TapLabelPosition label_position)
      : ActionLabel(mouse_action), label_position_(label_position) {
    DCHECK(mouse_action == MouseAction::PRIMARY_CLICK ||
           mouse_action == MouseAction::SECONDARY_CLICK);
  }

  ActionLabelTap(const std::u16string& text, TapLabelPosition label_position)
      : ActionLabel(text), label_position_(label_position) {}

  ~ActionLabelTap() override = default;

  void UpdateBounds() override {
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kSideInset)));
    const auto label_size = CalculatePreferredSize();
    SetSize(label_size);
    // Label position is not set yet.
    if (label_position_ == TapLabelPosition::kNone) {
      return;
    }

    auto* action_view = static_cast<ActionView*>(parent());

    switch (label_position_) {
      case TapLabelPosition::kBottomLeft:
        SetPosition(
            gfx::Point(0, touch_point_size_.height() + kOffsetToTouchPoint));
        action_view->SetTouchPointCenter(
            gfx::Point(label_size.width() + touch_point_size_.width() / 2 +
                           kOffsetToTouchPoint,
                       touch_point_size_.height() / 2));
        break;
      case TapLabelPosition::kBottomRight:
        SetPosition(
            gfx::Point(touch_point_size_.width() + kOffsetToTouchPoint,
                       touch_point_size_.height() + kOffsetToTouchPoint));
        action_view->SetTouchPointCenter(gfx::Point(
            touch_point_size_.width() / 2, touch_point_size_.height() / 2));
        break;
      case TapLabelPosition::kTopLeft:
        SetPosition(gfx::Point());
        action_view->SetTouchPointCenter(
            gfx::Point(label_size.width() + kOffsetToTouchPoint +
                           touch_point_size_.width() / 2,
                       label_size.height() + kOffsetToTouchPoint +
                           touch_point_size_.height() / 2));
        break;
      case TapLabelPosition::kTopRight:
        SetPosition(
            gfx::Point(touch_point_size_.width() + kOffsetToTouchPoint, 0));
        action_view->SetTouchPointCenter(gfx::Point(
            touch_point_size_.width() / 2, label_size.height() +
                                               kOffsetToTouchPoint +
                                               touch_point_size_.height() / 2));
        break;
      default:
        NOTREACHED();
    }
  }

  void UpdateLabelPositionType(TapLabelPosition label_position) override {
    if (label_position_ == label_position) {
      return;
    }

    parent()->SetPosition(
        CalculateParentPositionWithFixedTouchPoint(label_position));
    label_position_ = label_position;
    UpdateBounds();
  }

 private:
  gfx::Point CalculateParentPositionWithFixedTouchPoint(
      TapLabelPosition label_position) {
    DCHECK_NE(label_position_, label_position);
    DCHECK_NE(label_position, TapLabelPosition::kNone);
    auto* action_view = static_cast<ActionView*>(parent());
    auto fix_pos = action_view->GetTouchCenterInWindow();
    fix_pos.Offset(-touch_point_size_.width() / 2,
                   -touch_point_size_.height() / 2);
    fix_pos.SetToMax(gfx::Point());
    auto new_pos = action_view->origin();
    const auto& label_size = size();

    if (IsLeft(label_position_) && IsRight(label_position)) {
      new_pos.set_x(fix_pos.x());
    } else if (!IsLeft(label_position_) && IsLeft(label_position)) {
      new_pos.set_x(std::max(
          0, fix_pos.x() - (label_size.width() + kOffsetToTouchPoint)));
    }

    if (IsTop(label_position_) && IsBottom(label_position)) {
      new_pos.set_y(fix_pos.y());
    } else if (!IsTop(label_position_) && IsTop(label_position)) {
      new_pos.set_y(std::max(
          0, fix_pos.y() - (label_size.height() + kOffsetToTouchPoint)));
    }

    return new_pos;
  }

  TapLabelPosition label_position_ = TapLabelPosition::kNone;
};

class ActionLabelMove : public ActionLabel {
 public:
  ActionLabelMove(const std::u16string& text, size_t index)
      : ActionLabel(text, index) {}
  explicit ActionLabelMove(MouseAction mouse) : ActionLabel(mouse) {}

  ~ActionLabelMove() override = default;

  void UpdateBounds() override {
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 0)));
    auto label_size = CalculatePreferredSize();
    SetSize(label_size);
    // TODO(b/241966781): Mouse is not supported yet.
    DCHECK_EQ(mouse_action_, MouseAction::NONE);
    auto center = touch_point_size_.width() / 2;
    int offset_to_center =
        touch_point_size_.width() / 2 - kCrossPadding - label_size.height() / 2;
    int x = center + kDirection[index_][0] * offset_to_center -
            label_size.width() / 2;
    int y = center + kDirection[index_][1] * offset_to_center -
            label_size.height() / 2;
    SetPosition(gfx::Point(x, y));
    static_cast<ActionView*>(parent())->SetTouchPointCenter(
        gfx::Point(center, center));
  }

  void UpdateLabelPositionType(TapLabelPosition label_position) override {}
};

}  // namespace

std::vector<ActionLabel*> ActionLabel::Show(views::View* parent,
                                            ActionType action_type,
                                            const InputElement& input_element,
                                            TapLabelPosition label_position) {
  std::vector<ActionLabel*> labels;
  gfx::Size touch_point_size;

  switch (action_type) {
    case ActionType::TAP:
      if (IsKeyboardBound(input_element)) {
        DCHECK_EQ(1u, input_element.keys().size());
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelTap>(
                GetDisplayText(input_element.keys()[0]), label_position)));
      } else if (IsMouseBound(input_element)) {
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelTap>(
                input_element.mouse_action(), label_position)));
      } else {
        labels.emplace_back(parent->AddChildView(
            std::make_unique<ActionLabelTap>(kUnknownBind, label_position)));
      }
      touch_point_size = TouchPoint::GetSize(ActionType::TAP);
      break;

    case ActionType::MOVE:
      if (IsKeyboardBound(input_element)) {
        const auto& keys = input_element.keys();
        for (size_t i = 0; i < kActionMoveKeysSize; i++) {
          labels.emplace_back(parent->AddChildView(
              std::make_unique<ActionLabelMove>(GetDisplayText(keys[i]), i)));
        }
      } else if (IsMouseBound(input_element)) {
        labels.emplace_back(parent->AddChildView(
            std::make_unique<ActionLabelMove>(kMouseCursorLock, 0)));
        NOTIMPLEMENTED();
      } else {
        for (size_t i = 0; i < kActionMoveKeysSize; i++) {
          labels.emplace_back(parent->AddChildView(
              std::make_unique<ActionLabelMove>(kUnknownBind, i)));
        }
      }
      touch_point_size = TouchPoint::GetSize(ActionType::MOVE);
      break;

    default:
      NOTREACHED();
      break;
  }

  for (auto* label : labels) {
    label->Init();
    label->set_touch_point_size(touch_point_size);
  }

  return labels;
}

void ActionLabel::Init() {
  SetRequestFocusOnPress(true);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kSideInset)));
  SetAccessibilityProperties(ax::mojom::Role::kLabelText,
                             CalculateAccessibleName());
}

ActionLabel::ActionLabel(MouseAction mouse_action)
    : mouse_action_(mouse_action) {}

ActionLabel::ActionLabel(const std::u16string& text, size_t index)
    : views::LabelButton(views::Button::PressedCallback(), text),
      index_(index) {
  DCHECK(index_ >= 0 && index_ < kActionMoveKeysSize);
}

ActionLabel::~ActionLabel() = default;

void ActionLabel::SetTextActionLabel(const std::u16string& text) {
  label()->SetText(text);
  SetAccessibleName(CalculateAccessibleName());
}

void ActionLabel::SetImageActionLabel(MouseAction mouse_action) {
  set_mouse_action(mouse_action);
  SetAccessibleName(CalculateAccessibleName());
}

void ActionLabel::SetDisplayMode(DisplayMode mode) {
  DCHECK(mode != DisplayMode::kMenu && mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kMenu || mode == DisplayMode::kPreMenu) {
    return;
  }

  switch (mode) {
    case DisplayMode::kView:
      SetToViewMode();
      SetFocusBehavior(FocusBehavior::NEVER);
      break;
    case DisplayMode::kEdit:
      SetToEditMode();
      SetFocusBehavior(FocusBehavior::ALWAYS);
      static_cast<ActionView*>(parent())->ShowInfoMsg(
          l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS_ALPHAV2),
          this);
      break;
    case DisplayMode::kEditedSuccess:
      SetToEditFocus();
      break;
    case DisplayMode::kEditedUnbound:
      SetToEditUnbindInput();
      break;
    case DisplayMode::kEditedError:
      SetToEditError();
      break;
    case DisplayMode::kRestore:
      SetToEditDefault();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ActionLabel::ClearFocus() {
  if (!HasFocus()) {
    return;
  }

  auto* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return;
  }

  focus_manager->ClearFocus();
  // When it has to clear focus explicitly, set focused view back to its parent,
  // so it can find the focused view when Tab traversal key is pressed.
  focus_manager->SetFocusedView(parent());
}

void ActionLabel::OnSiblingUpdateFocus(bool sibling_focused) {
  if (sibling_focused) {
    SetToEditInactive();
  } else if (!IsInputUnbound()) {
    SetToEditDefault();
  } else {
    SetToEditUnbindInput();
  }
}

gfx::Size ActionLabel::CalculatePreferredSize() const {
  auto size = LabelButton::CalculatePreferredSize();
  size.SetToMax(kLabelSize);
  return size;
}

void ActionLabel::ChildPreferredSizeChanged(View* child) {
  UpdateBounds();
  LabelButton::ChildPreferredSizeChanged(this);
}

bool ActionLabel::OnKeyPressed(const ui::KeyEvent& event) {
  DCHECK(parent());
  auto code = event.code();
  auto* parent_view = static_cast<ActionView*>(parent());
  if (GetDisplayText(code) == GetText() ||
      parent_view->ShouldShowErrorMsg(code)) {
    return true;
  }

  parent_view->OnKeyBindingChange(this, code);
  return true;
}

void ActionLabel::OnMouseEntered(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus()) {
    SetToEditHover(true);
  }
}

void ActionLabel::OnMouseExited(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus()) {
    SetToEditHover(false);
  }
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  LabelButton::OnFocus();
  static_cast<ActionView*>(parent())->OnChildLabelUpdateFocus(this,
                                                              /*focus=*/true);

  if (IsInputUnbound()) {
    static_cast<ActionView*>(parent())->ShowErrorMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING), this,
        /*ax_annouce=*/false);
  } else {
    static_cast<ActionView*>(parent())->ShowFocusInfoMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_FOCUSED_KEY), this);
  }
}

void ActionLabel::OnBlur() {
  SetToEditDefault();
  LabelButton::OnBlur();
  static_cast<ActionView*>(parent())->OnChildLabelUpdateFocus(this,
                                                              /*focus=*/false);
  static_cast<ActionView*>(parent())->RemoveMessage();
}

void ActionLabel::SetToViewMode() {
  display_mode_ = DisplayMode::kView;
  ClearFocus();
  SetInstallFocusRingOnFocus(false);
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
  SetEnabledTextColors(kTextColorDefault);

  if (mouse_action_ != MouseAction::NONE) {
    if (mouse_action_ == MouseAction::PRIMARY_CLICK) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickViewIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
  }

  SetBackground(views::CreateRoundedRectBackground(kBackgroundColorDefault,
                                                   kCornerRadius));
  SetPreferredSize(CalculatePreferredSize());
}

void ActionLabel::SetToEditMode() {
  display_mode_ = DisplayMode::kEdit;

  if (IsInputUnbound()) {
    SetVisible(true);
  }

  SetInstallFocusRingOnFocus(true);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        return view->IsMouseHovered() || view->HasFocus();
      }));

  SetEnabledTextColors(kTextColorDefault);

  if (mouse_action_ != MouseAction::NONE) {
    if (mouse_action_ == MouseAction::PRIMARY_CLICK) {
      auto left_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseLeftClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, left_click_icon);
    } else {
      auto right_click_icon = gfx::CreateVectorIcon(
          gfx::IconDescription(kMouseRightClickEditIcon, kIconSize));
      SetImage(views::Button::STATE_NORMAL, right_click_icon);
    }
  }
  SetToEditDefault();
}

void ActionLabel::SetToEditDefault() {
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
  SetEnabledTextColors(kTextColorDefault);
  SetBackgroundForEdit();
  views::FocusRing::Get(this)->SetColorId(absl::nullopt);
}

void ActionLabel::SetToEditHover(bool hovered) {
  if (hovered) {
    views::FocusRing::Get(this)->SetColorId(
        ui::kColorAshActionLabelFocusRingHover);
  } else {
    views::FocusRing::Get(this)->SetColorId(absl::nullopt);
  }
}

void ActionLabel::SetToEditFocus() {
  label()->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL, kFontSize,
                                     gfx::Font::Weight::BOLD));
  SetPreferredSize(CalculatePreferredSize());
  SetEnabledTextColors(kTextColorDefault);
  SetBackgroundForEdit();
  views::FocusRing::Get(this)->SetColorId(
      IsInputUnbound() ? ui::kColorAshActionLabelFocusRingError
                       : ui::kColorAshActionLabelFocusRingEdit);
}

void ActionLabel::SetToEditError() {
  views::FocusRing::Get(this)->SetColorId(
      ui::kColorAshActionLabelFocusRingError);
}

void ActionLabel::SetToEditUnbindInput() {
  SetPreferredSize(CalculatePreferredSize());
  SetBackground(
      views::CreateRoundedRectBackground(kEditedUnboundBgColor, kCornerRadius));
}

void ActionLabel::SetToEditInactive() {
  if (IsInputUnbound()) {
    return;
  }

  SetBackground(nullptr);
  SetEnabledTextColors(kEditInactiveTextColor);
}

void ActionLabel::SetBackgroundForEdit() {
  SetBackground(views::CreateRoundedRectBackground(
      IsInputUnbound() ? kEditedUnboundBgColor : kBackgroundColorDefault,
      kCornerRadius));
}

bool ActionLabel::IsInputUnbound() {
  return GetText().compare(kUnknownBind) == 0;
}

std::u16string ActionLabel::CalculateAccessibleName() {
  if (mouse_action_ != MouseAction::NONE) {
    // TODO(accessibility): The accessible name is expected to be end-user
    // consumable.
    return base::UTF8ToUTF16(GetClassName());
  }

  return l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_KEYMAPPING_KEY)
      .append(u" ")
      .append(GetDisplayTextAccessibleName(label()->GetText()));
}

BEGIN_METADATA(ActionLabel, views::LabelButton)
END_METADATA

}  // namespace arc::input_overlay
