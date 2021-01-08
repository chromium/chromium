// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_menu_view.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

constexpr int kButtonSize = 36;
constexpr int kStopButtonPadding = 14;
constexpr int kSeparatorHeight = 16;

SelectToSpeakPanelAction PanelActionForButtonID(int button_id, bool is_paused) {
  auto button_enum = static_cast<SelectToSpeakMenuView::ButtonId>(button_id);
  switch (button_enum) {
    case SelectToSpeakMenuView::ButtonId::kPrevParagraph:
      return SelectToSpeakPanelAction::kPreviousParagraph;
    case SelectToSpeakMenuView::ButtonId::kPrevSentence:
      return SelectToSpeakPanelAction::kPreviousSentence;
    case SelectToSpeakMenuView::ButtonId::kPause:
      // Pause button toggles pause/resume state.
      if (is_paused)
        return SelectToSpeakPanelAction::kResume;
      else
        return SelectToSpeakPanelAction::kPause;
    case SelectToSpeakMenuView::ButtonId::kNextParagraph:
      return SelectToSpeakPanelAction::kNextParagraph;
    case SelectToSpeakMenuView::ButtonId::kNextSentence:
      return SelectToSpeakPanelAction::kNextSentence;
    case SelectToSpeakMenuView::ButtonId::kStop:
      return SelectToSpeakPanelAction::kExit;
    case SelectToSpeakMenuView::ButtonId::kSpeed:
      return SelectToSpeakPanelAction::kChangeSpeed;
  }

  NOTREACHED();
  return SelectToSpeakPanelAction::kNone;
}

}  // namespace

SelectToSpeakMenuView::SelectToSpeakMenuView(Delegate* delegate)
    : delegate_(delegate) {
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  views::Builder<SelectToSpeakMenuView>(this)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kEnd)
      .AddChildren(
          {views::Builder<views::BoxLayoutView>()
               .SetInsideBorderInsets(kUnifiedMenuItemPadding)
               .SetBetweenChildSpacing(kUnifiedTopShortcutSpacing)
               .AddChildren(
                   {views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&prev_paragraph_button_)
                        .SetID(static_cast<int>(ButtonId::kPrevParagraph))
                        .SetVectorIcon(kSelectToSpeakPrevParagraphIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_PREV_PARAGRAPH))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(prev_paragraph_button_))),
                    views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&prev_sentence_button_)
                        .SetID(static_cast<int>(ButtonId::kPrevSentence))
                        .SetVectorIcon(kSelectToSpeakPrevSentenceIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_PREV_SENTENCE))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(prev_sentence_button_))),
                    views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&pause_button_)
                        .SetID(static_cast<int>(ButtonId::kPause))
                        .SetVectorIcon(kSelectToSpeakPauseIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_PAUSE))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(pause_button_))),
                    views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&next_sentence_button_)
                        .SetID(static_cast<int>(ButtonId::kNextSentence))
                        .SetVectorIcon(kSelectToSpeakNextSentenceIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_NEXT_SENTENCE))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(next_sentence_button_))),
                    views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&next_paragraph_button_)
                        .SetID(static_cast<int>(ButtonId::kNextParagraph))
                        .SetVectorIcon(kSelectToSpeakNextParagraphIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_NEXT_PARAGRAPH))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(next_paragraph_button_))),
                    views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&speed_button_)
                        .SetID(static_cast<int>(ButtonId::kSpeed))
                        .SetVectorIcon(kSelectToSpeakReadingSpeedNormalIcon)
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_READING_SPEED))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(speed_button_)))}),
           views::Builder<views::Separator>()
               .SetColor(AshColorProvider::Get()->GetContentLayerColor(
                   AshColorProvider::ContentLayerType::kSeparatorColor))
               .SetPreferredHeight(kSeparatorHeight)
               .SetBorder(views::CreateEmptyBorder(
                   separator_spacing - kUnifiedTopShortcutSpacing, 0,
                   separator_spacing, 0)),
           views::Builder<views::BoxLayoutView>()
               .SetInsideBorderInsets(gfx::Insets(0, kStopButtonPadding,
                                                  kStopButtonPadding,
                                                  kStopButtonPadding))
               .SetBetweenChildSpacing(kStopButtonPadding)
               .AddChildren(
                   {views::Builder<FloatingMenuButton>()
                        .CopyAddressTo(&stop_button_)
                        .SetID(static_cast<int>(ButtonId::kStop))
                        .SetVectorIcon(kSelectToSpeakStopIcon)
                        .SetPreferredSize(gfx::Size(kButtonSize, kButtonSize))
                        .SetTooltipText(l10n_util::GetStringUTF16(
                            IDS_ASH_SELECT_TO_SPEAK_EXIT))
                        .SetCallback(base::BindRepeating(
                            &SelectToSpeakMenuView::OnButtonPressed,
                            base::Unretained(this),
                            base::Unretained(stop_button_)))})})
      .BuildChildren();
}

void SelectToSpeakMenuView::SetInitialSpeechRate(double initial_speech_rate) {
  const gfx::VectorIcon* speed_icon = &kSelectToSpeakReadingSpeedNormalIcon;
  if (initial_speech_rate == kSelectToSpeakSpeechRateSlow) {
    speed_icon = &kSelectToSpeakReadingSpeedSlowIcon;
  } else if (initial_speech_rate == kSelectToSpeakSpeechRatePeppy) {
    speed_icon = &kSelectToSpeakReadingSpeedPeppyIcon;
  } else if (initial_speech_rate == kSelectToSpeakSpeechRateFast) {
    speed_icon = &kSelectToSpeakReadingSpeedFastIcon;
  } else if (initial_speech_rate == kSelectToSpeakSpeechRateFaster) {
    speed_icon = &kSelectToSpeakReadingSpeedFasterIcon;
  }
  speed_button_->SetVectorIcon(*speed_icon);
}

void SelectToSpeakMenuView::OnKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->type() != ui::ET_KEY_PRESSED || key_event->is_repeat()) {
    // Only process key when first pressed.
    return;
  }

  auto action = SelectToSpeakPanelAction::kNone;
  switch (key_event->key_code()) {
    case ui::KeyboardCode::VKEY_LEFT:
      action = SelectToSpeakPanelAction::kPreviousSentence;
      break;
    case ui::KeyboardCode::VKEY_RIGHT:
      action = SelectToSpeakPanelAction::kNextSentence;
      break;
    case ui::KeyboardCode::VKEY_UP:
      action = SelectToSpeakPanelAction::kPreviousParagraph;
      break;
    case ui::KeyboardCode::VKEY_DOWN:
      action = SelectToSpeakPanelAction::kNextParagraph;
      break;
    case ui::KeyboardCode::VKEY_ESCAPE:
      action = SelectToSpeakPanelAction::kExit;
      break;
    default:
      // Unhandled key.
      return;
  }
  delegate_->OnActionSelected(action);
  key_event->SetHandled();
  key_event->StopPropagation();
}

void SelectToSpeakMenuView::SetPaused(bool is_paused) {
  pause_button_->SetVectorIcon(is_paused ? kSelectToSpeakPlayIcon
                                         : kSelectToSpeakPauseIcon);
  pause_button_->SetTooltipText(
      l10n_util::GetStringUTF16(is_paused ? IDS_ASH_SELECT_TO_SPEAK_RESUME
                                          : IDS_ASH_SELECT_TO_SPEAK_PAUSE));
  is_paused_ = is_paused;
}

void SelectToSpeakMenuView::SetInitialFocus() {
  pause_button_->RequestFocus();
}

void SelectToSpeakMenuView::SetSpeedButtonFocused() {
  speed_button_->RequestFocus();
}

void SelectToSpeakMenuView::SetSpeedButtonToggled(bool toggled) {
  speed_button_->SetToggled(toggled);
}

void SelectToSpeakMenuView::OnButtonPressed(views::Button* sender) {
  SelectToSpeakPanelAction action =
      PanelActionForButtonID(sender->GetID(), is_paused_);
  delegate_->OnActionSelected(action);
}

BEGIN_METADATA(SelectToSpeakMenuView, views::BoxLayoutView)
END_METADATA

}  // namespace ash