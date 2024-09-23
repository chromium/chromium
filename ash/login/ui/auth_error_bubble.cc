// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_error_bubble.h"

#include <optional>
#include <string>

#include "ash/ime/ime_controller_impl.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Horizontal and vertical padding of auth error bubble.
constexpr int kHorizontalPaddingAuthErrorBubbleDp = 8;
constexpr int kVerticalPaddingAuthErrorBubbleDp = 8;

// Spacing between child of LoginBaseBubbleView.
constexpr int kBubbleBetweenChildSpacingDp = 16;

constexpr char kAuthErrorContainerName[] = "AuthErrorContainer";

// Make a section of the text bold.
// |label|:       The label to apply mixed styles.
// |text|:        The message to display.
// |bold_start|:  The position in |text| to start bolding.
// |bold_length|: The length of bold text.
void MakeSectionBold(views::StyledLabel* label,
                     const std::u16string& text,
                     const std::optional<int>& bold_start,
                     int bold_length) {
  auto create_style = [&](bool is_bold) {
    views::StyledLabel::RangeStyleInfo style;
    if (is_bold) {
      style.custom_font = label->GetFontList().Derive(
          0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD);
    }
    style.override_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    return style;
  };

  auto add_style = [&](const views::StyledLabel::RangeStyleInfo& style,
                       int start, int end) {
    if (start >= end) {
      return;
    }

    label->AddStyleRange(gfx::Range(start, end), style);
  };

  views::StyledLabel::RangeStyleInfo regular_style =
      create_style(false /*is_bold*/);
  views::StyledLabel::RangeStyleInfo bold_style =
      create_style(true /*is_bold*/);
  if (!bold_start || bold_length == 0) {
    add_style(regular_style, 0, text.length());
    return;
  }

  add_style(regular_style, 0, *bold_start - 1);
  add_style(bold_style, *bold_start, *bold_start + bold_length);
  add_style(regular_style, *bold_start + bold_length + 1, text.length());
}

}  // namespace

AuthErrorBubble::AuthErrorBubble(
    const base::RepeatingClosure& on_learn_more_button_pressed,
    const base::RepeatingClosure& on_recover_button_pressed)
    : on_learn_more_button_pressed_(on_learn_more_button_pressed),
      on_recover_button_pressed_(on_recover_button_pressed) {
  set_positioning_strategy(PositioningStrategy::kTryAfterThenBefore);
  SetPadding(kHorizontalPaddingAuthErrorBubbleDp,
             kVerticalPaddingAuthErrorBubbleDp);
}

AuthErrorBubble::~AuthErrorBubble() {}

void AuthErrorBubble::ShowAuthError(base::WeakPtr<views::View> anchor_view,
                                    int unlock_attempt,
                                    bool authenticated_by_pin,
                                    bool is_login_screen) {
  std::u16string error_text;
  if (authenticated_by_pin) {
    error_text += l10n_util::GetStringUTF16(
        unlock_attempt > 1 ? IDS_ASH_LOGIN_ERROR_AUTHENTICATING_PIN_2ND_TIME
                           : IDS_ASH_LOGIN_ERROR_AUTHENTICATING_PIN);
  } else {
    error_text += l10n_util::GetStringUTF16(
        unlock_attempt > 1 ? IDS_ASH_LOGIN_ERROR_AUTHENTICATING_PWD_2ND_TIME
                           : IDS_ASH_LOGIN_ERROR_AUTHENTICATING_PWD);
  }

  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  if (ime_controller->IsCapsLockEnabled()) {
    base::StrAppend(
        &error_text,
        {u" ", l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_CAPS_LOCK_HINT)});
  }

  std::optional<int> bold_start;
  int bold_length = 0;
  // Display a hint to switch keyboards if there are other active input
  // methods in clamshell mode.
  if (ime_controller->GetVisibleImes().size() > 1 &&
      !display::Screen::GetScreen()->InTabletMode()) {
    error_text += u" ";
    bold_start = error_text.length();
    std::u16string shortcut =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SWITCH_SHORTCUT);
    bold_length = shortcut.length();

    size_t shortcut_offset_in_string;
    error_text +=
        l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_ERROR_KEYBOARD_SWITCH_HINT,
                                   shortcut, &shortcut_offset_in_string);
    *bold_start += shortcut_offset_in_string;
  }

  if (unlock_attempt > 1) {
    base::StrAppend(&error_text,
                    {u"\n\n", l10n_util::GetStringUTF16(
                                  authenticated_by_pin
                                      ? IDS_ASH_LOGIN_ERROR_RECOVER_USER
                                      : IDS_ASH_LOGIN_ERROR_RECOVER_USER_PWD)});
  }

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(error_text);
  MakeSectionBold(label.get(), error_text, bold_start, bold_length);
  label->SetAutoColorReadabilityEnabled(false);

  auto learn_more_button = std::make_unique<PillButton>(
      base::BindRepeating(&AuthErrorBubble::OnLearnMoreButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));

  auto container = std::make_unique<NonAccessibleView>(kAuthErrorContainerName);
  auto* container_layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kBubbleBetweenChildSpacingDp));
  container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  label_ = container->AddChildView(std::move(label));
  learn_more_button_ = container->AddChildView(std::move(learn_more_button));

  // The recover user flow is only accessible from the login screen but
  // not from the lock screen.
  if (is_login_screen &&
      Shell::Get()->session_controller()->GetSessionState() !=
          session_manager::SessionState::LOGIN_SECONDARY) {
    auto recover_user_button = std::make_unique<PillButton>(
        base::BindRepeating(&AuthErrorBubble::OnRecoverButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_RECOVER_USER_BUTTON));

    container->AddChildView(std::move(recover_user_button));
  }

  SetAnchorView(anchor_view);
  SetContent(std::move(container));

  // We set an accessible name when content is not accessible. This happens if
  // content is a container (e.g. a text and a "learn more" button). In such a
  // case, it will have multiple subviews but only one which needs to be read
  // on bubble show – when the alert event occurs.
  GetViewAccessibility().SetName(error_text);
  Show();
}

void AuthErrorBubble::OnLearnMoreButtonPressed() {
  on_learn_more_button_pressed_.Run();
}

void AuthErrorBubble::OnRecoverButtonPressed() {
  on_recover_button_pressed_.Run();
}

BEGIN_METADATA(AuthErrorBubble)
END_METADATA

}  // namespace ash
