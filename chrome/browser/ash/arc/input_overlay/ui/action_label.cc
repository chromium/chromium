// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"

#include <set>

#include "ash/style/style_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
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
// UI specs - Alpha.
constexpr int kSideInsetAlpha = 6;
constexpr gfx::Size kLabelSizeAlpha(32, 32);
constexpr int kLabelMarginAlpha = 2;
constexpr int kCornerRadiusAlpha = 6;
constexpr int kFontSizeAlpha = 16;
// For ActionTap.
constexpr int kLabelPositionToSideAlpha = 36;
// For ActionMove.
constexpr int kLabelOffsetAlpha = 49;
// About focus ring - Alpha.
// Gap between focus ring outer edge to label.
constexpr float kHaloInsetAlpha = -6;
// Thickness of focus ring.
constexpr float kHaloThicknessAlpha = 4;

constexpr char kFontStyle[] = "Google Sans";
constexpr int kIconSize = 20;

// TODO(b/260937747): Remove colors for Alpha when AlphaV2 flag is removed.
// About colors - Alpha.
constexpr SkColor kViewModeForeColorAlpha = SkColorSetA(SK_ColorBLACK, 0x29);
constexpr SkColor kViewModeBackColorAlpha =
    SkColorSetA(gfx::kGoogleGrey800, 0xCC);
constexpr SkColor kViewTextColorAlpha = SK_ColorWHITE;

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
constexpr char kMouseCursorLock[] = "mouse cursor lock (esc)";
constexpr char kUnknownBind[] = "?";

// Arrow symbols for arrow keys.
constexpr char kLeftArrow[] = "←";
constexpr char kUpArrow[] = "↑";
constexpr char kRightArrow[] = "→";
constexpr char kDownArrow[] = "↓";
constexpr char kBackQuote[] = "`";
constexpr char kMinus[] = "-";
constexpr char kEqual[] = "=";
constexpr char kBracketLeft[] = "[";
constexpr char kBracketRight[] = "]";
constexpr char kBackSlash[] = "\\";
constexpr char kSemicolon[] = ";";
constexpr char kQuote[] = "'";
constexpr char kComma[] = ",";
constexpr char kPeriod[] = ".";
constexpr char kSlash[] = "/";
constexpr char kBackSpace[] = "back";
constexpr char kEnter[] = "enter";
constexpr char kEscape[] = "esc";

// Modifier keys.
constexpr char kAlt[] = "alt";
constexpr char kCtrl[] = "ctrl";
constexpr char kShift[] = "shift";
constexpr char kCap[] = "cap";

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
  ActionLabelTap(int radius,
                 MouseAction mouse_action,
                 TapLabelPosition label_position,
                 bool allow_reposition)
      : ActionLabel(radius, mouse_action, allow_reposition),
        label_position_(label_position) {
    DCHECK(mouse_action == MouseAction::PRIMARY_CLICK ||
           mouse_action == MouseAction::SECONDARY_CLICK);
  }

  ActionLabelTap(int radius,
                 const std::string& text,
                 TapLabelPosition label_position,
                 bool allow_reposition)
      : ActionLabel(radius, text, allow_reposition),
        label_position_(label_position) {}

  ~ActionLabelTap() override = default;

  void UpdateBoundsAlpha() override {
    auto label_size = CalculatePreferredSize();
    SetSize(label_size);
    int width =
        std::max(radius_ * 2 - kLabelPositionToSideAlpha + label_size.width(),
                 radius_ * 2);
    switch (label_position_) {
      case TapLabelPosition::kBottomLeft:
        SetPosition(gfx::Point(
            0, radius_ * 2 - label_size.height() - kLabelMarginAlpha));
        static_cast<ActionView*>(parent())->SetTouchPointCenter(
            gfx::Point(width - radius_, radius_));
        break;
      case TapLabelPosition::kBottomRight:
        SetPosition(
            gfx::Point(label_size.width() > kLabelPositionToSideAlpha
                           ? width - label_size.width()
                           : width - kLabelPositionToSideAlpha,
                       radius_ * 2 - label_size.height() - kLabelMarginAlpha));
        static_cast<ActionView*>(parent())->SetTouchPointCenter(
            gfx::Point(radius_, radius_));
        break;
      default:
        NOTREACHED();
    }
  }

  void UpdateBounds() override {
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kSideInset)));
    const auto label_size = CalculatePreferredSize();
    SetSize(label_size);
    // Label position is not set yet.
    if (label_position_ == TapLabelPosition::kNone)
      return;

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
    if (label_position_ == label_position)
      return;

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
  ActionLabelMove(int radius,
                  const std::string& text,
                  int index,
                  bool allow_reposition)
      : ActionLabel(radius, text, index, allow_reposition) {}
  ActionLabelMove(int radius, MouseAction mouse, bool allow_reposition)
      : ActionLabel(radius, mouse, allow_reposition) {}

  ~ActionLabelMove() override = default;

  void UpdateBoundsAlpha() override {
    auto label_size = CalculatePreferredSize();
    SetSize(label_size);
    if (mouse_action_ == MouseAction::NONE) {
      int x = kDirection[index_][0];
      int y = kDirection[index_][1];
      SetPosition(gfx::Point(
          radius_ + x * (radius_ - kLabelOffsetAlpha) - label_size.width() / 2,
          radius_ + y * (radius_ - kLabelOffsetAlpha) -
              label_size.height() / 2));
    } else {
      SetPosition(gfx::Point());
      static_cast<ActionView*>(parent())->SetTouchPointCenter(
          gfx::Point(label_size.width() / 2, label_size.height() / 2));
    }
  }

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

std::string GetDisplayText(const ui::DomCode code) {
  switch (code) {
    case ui::DomCode::NONE:
      return kUnknownBind;
    case ui::DomCode::ARROW_LEFT:
      return kLeftArrow;
    case ui::DomCode::ARROW_RIGHT:
      return kRightArrow;
    case ui::DomCode::ARROW_UP:
      return kUpArrow;
    case ui::DomCode::ARROW_DOWN:
      return kDownArrow;
    case ui::DomCode::BACKQUOTE:
      return kBackQuote;
    case ui::DomCode::MINUS:
      return kMinus;
    case ui::DomCode::EQUAL:
      return kEqual;
    case ui::DomCode::BRACKET_LEFT:
      return kBracketLeft;
    case ui::DomCode::BRACKET_RIGHT:
      return kBracketRight;
    case ui::DomCode::BACKSLASH:
      return kBackSlash;
    case ui::DomCode::SEMICOLON:
      return kSemicolon;
    case ui::DomCode::QUOTE:
      return kQuote;
    case ui::DomCode::COMMA:
      return kComma;
    case ui::DomCode::PERIOD:
      return kPeriod;
    case ui::DomCode::SLASH:
      return kSlash;
    case ui::DomCode::BACKSPACE:
      return kBackSpace;
    case ui::DomCode::ENTER:
      return kEnter;
    case ui::DomCode::ESCAPE:
      return kEscape;
    // Modifier keys.
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return kAlt;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return kCtrl;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return kShift;
    case ui::DomCode::CAPS_LOCK:
      return kCap;
    default:
      std::string dom_code_string =
          ui::KeycodeConverter::DomCodeToCodeString(code);
      if (base::StartsWith(dom_code_string, "Key",
                           base::CompareCase::SENSITIVE))
        return base::ToLowerASCII(dom_code_string.substr(3));
      if (base::StartsWith(dom_code_string, "Digit",
                           base::CompareCase::SENSITIVE))
        return dom_code_string.substr(5);
      // TODO(cuicuiruan): better display for number pad. Current it shows in
      // the format of "numpad1" since the number keys on number pad are not
      // considered the same as numbers on the main keyboard.
      auto lower = base::ToLowerASCII(dom_code_string);
      return lower;
  }
}

std::vector<ActionLabel*> ActionLabel::Show(views::View* parent,
                                            ActionType action_type,
                                            const InputElement& input_element,
                                            int radius,
                                            bool allow_reposition,
                                            TapLabelPosition label_position) {
  std::vector<ActionLabel*> labels;
  gfx::Size touch_point_size;

  switch (action_type) {
    case ActionType::TAP:
      if (IsKeyboardBound(input_element)) {
        DCHECK_EQ(1u, input_element.keys().size());
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelTap>(
                radius, GetDisplayText(input_element.keys()[0]), label_position,
                allow_reposition)));
      } else if (IsMouseBound(input_element)) {
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelTap>(
                radius, input_element.mouse_action(), label_position,
                allow_reposition)));
      } else {
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelTap>(
                radius, kUnknownBind, label_position, allow_reposition)));
      }
      touch_point_size = TouchPoint::GetSize(ActionType::TAP);
      break;

    case ActionType::MOVE:
      if (IsKeyboardBound(input_element)) {
        const auto& keys = input_element.keys();
        for (size_t i = 0; i < kActionMoveKeysSize; i++) {
          labels.emplace_back(
              parent->AddChildView(std::make_unique<ActionLabelMove>(
                  radius, GetDisplayText(keys[i]), i, allow_reposition)));
        }
      } else if (IsMouseBound(input_element)) {
        labels.emplace_back(
            parent->AddChildView(std::make_unique<ActionLabelMove>(
                radius, kMouseCursorLock, 0, allow_reposition)));
        NOTIMPLEMENTED();
      } else {
        for (size_t i = 0; i < kActionMoveKeysSize; i++) {
          labels.emplace_back(
              parent->AddChildView(std::make_unique<ActionLabelMove>(
                  radius, kUnknownBind, i, allow_reposition)));
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
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, allow_reposition_ ? kSideInset : kSideInsetAlpha)));
  SetAccessibleName(mouse_action_ == MouseAction::NONE
                        ? label()->GetText()
                        : base::UTF8ToUTF16(GetClassName()));
}

ActionLabel::ActionLabel(int radius,
                         MouseAction mouse_action,
                         bool allow_reposition)
    : radius_(radius),
      mouse_action_(mouse_action),
      allow_reposition_(allow_reposition) {}

ActionLabel::ActionLabel(int radius,
                         const std::string& text,
                         bool allow_reposition)
    : views::LabelButton(views::Button::PressedCallback(),
                         base::UTF8ToUTF16(text)),
      radius_(radius),
      allow_reposition_(allow_reposition) {}

ActionLabel::ActionLabel(int radius,
                         const std::string& text,
                         int index,
                         bool allow_reposition)
    : views::LabelButton(views::Button::PressedCallback(),
                         base::UTF8ToUTF16(text)),
      radius_(radius),
      index_(index),
      allow_reposition_(allow_reposition) {
  DCHECK(index_ >= 0 && index_ < kActionMoveKeysSize);
}

ActionLabel::~ActionLabel() = default;

void ActionLabel::SetTextActionLabel(const std::string& text) {
  label()->SetText(base::UTF8ToUTF16(text));
  SetAccessibleName(label()->GetText());
}

void ActionLabel::SetImageActionLabel(MouseAction mouse_action) {
  SetAccessibleName(base::UTF8ToUTF16(GetClassName()));
  set_mouse_action(mouse_action);
}

void ActionLabel::SetDisplayMode(DisplayMode mode) {
  DCHECK(mode != DisplayMode::kMenu && mode != DisplayMode::kPreMenu);
  if (mode == DisplayMode::kMenu || mode == DisplayMode::kPreMenu)
    return;

  switch (mode) {
    case DisplayMode::kView:
      SetToViewMode();
      SetFocusBehavior(FocusBehavior::NEVER);
      break;
    case DisplayMode::kEdit:
      SetToEditMode();
      SetFocusBehavior(FocusBehavior::ALWAYS);
      static_cast<ActionView*>(parent())->ShowInfoMsg(
          l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_INSTRUCTIONS), this);
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
  if (!HasFocus())
    return;

  auto* focus_manager = GetFocusManager();
  if (!focus_manager)
    return;

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
  size.SetToMax(allow_reposition_ ? kLabelSize : kLabelSizeAlpha);
  return size;
}

void ActionLabel::ChildPreferredSizeChanged(View* child) {
  allow_reposition_ ? UpdateBounds() : UpdateBoundsAlpha();
  LabelButton::ChildPreferredSizeChanged(this);
}

bool ActionLabel::OnKeyPressed(const ui::KeyEvent& event) {
  DCHECK(parent());
  auto code = event.code();
  auto* parent_view = static_cast<ActionView*>(parent());
  if (base::UTF8ToUTF16(GetDisplayText(code)) == GetText() ||
      parent_view->ShouldShowErrorMsg(code)) {
    return true;
  }

  parent_view->OnKeyBindingChange(this, code);
  return true;
}

void ActionLabel::OnMouseEntered(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus())
    SetToEditHover(true);
}

void ActionLabel::OnMouseExited(const ui::MouseEvent& event) {
  if (IsFocusable() && !HasFocus())
    SetToEditHover(false);
}

void ActionLabel::OnFocus() {
  SetToEditFocus();
  LabelButton::OnFocus();
  if (allow_reposition_) {
    static_cast<ActionView*>(parent())->OnChildLabelUpdateFocus(this,
                                                                /*focus=*/true);
  }

  if (IsInputUnbound()) {
    static_cast<ActionView*>(parent())->ShowErrorMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_MISSING_BINDING), this,
        /*ax_annouce=*/false);
  } else {
    static_cast<ActionView*>(parent())->ShowLabelFocusInfoMsg(
        l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_FOCUSED_KEY), this);
  }
}

void ActionLabel::OnBlur() {
  SetToEditDefault();
  LabelButton::OnBlur();
  if (allow_reposition_) {
    static_cast<ActionView*>(parent())->OnChildLabelUpdateFocus(
        this, /*focus=*/false);
  }
  static_cast<ActionView*>(parent())->RemoveMessage();
}

void ActionLabel::SetToViewMode() {
  display_mode_ = DisplayMode::kView;
  ClearFocus();
  SetInstallFocusRingOnFocus(false);
  label()->SetFontList(gfx::FontList(
      {kFontStyle}, gfx::Font::NORMAL,
      allow_reposition_ ? kFontSize : kFontSizeAlpha, gfx::Font::Weight::BOLD));
  SetEnabledTextColors(allow_reposition_ ? kTextColorDefault
                                         : kViewTextColorAlpha);

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

  SetBackground(views::CreateRoundedRectBackground(
      allow_reposition_ ? kBackgroundColorDefault
                        : color_utils::GetResultingPaintColor(
                              kViewModeForeColorAlpha, kViewModeBackColorAlpha),
      allow_reposition_ ? kCornerRadius : kCornerRadiusAlpha));
  SetPreferredSize(CalculatePreferredSize());
}

void ActionLabel::SetToEditMode() {
  display_mode_ = DisplayMode::kEdit;

  if (IsInputUnbound())
    SetVisible(true);

  SetInstallFocusRingOnFocus(true);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(),
      allow_reposition_ ? kCornerRadius : kCornerRadiusAlpha);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(allow_reposition_ ? kHaloInset : kHaloInsetAlpha);
  focus_ring->SetHaloThickness(allow_reposition_ ? kHaloThickness
                                                 : kHaloThicknessAlpha);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return view->IsMouseHovered() || view->HasFocus();
  });

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
  label()->SetFontList(gfx::FontList(
      {kFontStyle}, gfx::Font::NORMAL,
      allow_reposition_ ? kFontSize : kFontSizeAlpha, gfx::Font::Weight::BOLD));
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
  label()->SetFontList(gfx::FontList(
      {kFontStyle}, gfx::Font::NORMAL,
      allow_reposition_ ? kFontSize : kFontSizeAlpha, gfx::Font::Weight::BOLD));
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
  SetBackground(views::CreateRoundedRectBackground(
      kEditedUnboundBgColor,
      allow_reposition_ ? kCornerRadius : kCornerRadiusAlpha));
}

void ActionLabel::SetToEditInactive() {
  if (IsInputUnbound())
    return;

  SetBackground(nullptr);
  SetEnabledTextColors(kEditInactiveTextColor);
}

void ActionLabel::SetBackgroundForEdit() {
  SetBackground(views::CreateRoundedRectBackground(
      IsInputUnbound() ? kEditedUnboundBgColor : kBackgroundColorDefault,
      allow_reposition_ ? kCornerRadius : kCornerRadiusAlpha));
}

bool ActionLabel::IsInputUnbound() {
  return base::UTF16ToUTF8(GetText()) == kUnknownBind;
}

}  // namespace arc::input_overlay
