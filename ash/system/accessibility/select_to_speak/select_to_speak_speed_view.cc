// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_constants.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Start offset in pixels to use for option views.
constexpr int kOptionInset = 16;

void RecordSpeedMetric(int value) {
  base::UmaHistogramSparse("Accessibility.CrosSelectToSpeak.SpeedSetFromBubble",
                           value);
}

}  // namespace

SelectToSpeakSpeedView::SelectToSpeakSpeedView(Delegate* delegate,
                                               double initial_speech_rate)
    : delegate_(delegate) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInitialSpeechRate(initial_speech_rate);
}

void SelectToSpeakSpeedView::SetInitialSpeechRate(double initial_speech_rate) {
  RemoveAllChildViews();

  for (size_t i = 0; i < std::size(kSelectToSpeakSpeechRates); i++) {
    double option_speed = kSelectToSpeakSpeechRates[i];
    bool is_selected = option_speed == initial_speech_rate;
    // Add 1 to the index, because view IDs cannot be 0.
    auto label = base::ASCIIToUTF16(base::StringPrintf("%.1fx", option_speed));
    AddMenuItem(/*option_id=*/i + 1, label, is_selected);
  }
}

void SelectToSpeakSpeedView::AddMenuItem(int option_id,
                                         const std::u16string& label,
                                         bool is_selected) {
  HoverHighlightView* item = new HoverHighlightView(this);
  AddChildView(item);
  item->AddLabelRow(label, kOptionInset);
  item->SetID(option_id);
  TrayPopupUtils::InitializeAsCheckableRow(item, is_selected, false);
}

void SelectToSpeakSpeedView::OnViewClicked(views::View* sender) {
  unsigned int speed_index = sender->GetID() - 1;
  double speech_rate = kSelectToSpeakSpeechRates[speed_index];
  if (speed_index >= 0 && speed_index < std::size(kSelectToSpeakSpeechRates)) {
    delegate_->OnSpeechRateSelected(speech_rate);
    RecordSpeedMetric(floor(speech_rate * 100));
  }
}

void SelectToSpeakSpeedView::SetInitialFocus() {
  if (children().size() == 0)
    return;

  children()[0]->RequestFocus();
}

void SelectToSpeakSpeedView::OnKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->type() != ui::EventType::kKeyPressed ||
      key_event->is_repeat()) {
    // Only process key when first pressed.
    return;
  }

  switch (key_event->key_code()) {
    case ui::KeyboardCode::VKEY_UP:
      GetFocusManager()->AdvanceFocus(/* reverse= */ true);
      break;
    case ui::KeyboardCode::VKEY_DOWN:
      GetFocusManager()->AdvanceFocus(/* reverse= */ false);
      break;
    default:
      // Unhandled key.
      return;
  }
  key_event->SetHandled();
  key_event->StopPropagation();
}

BEGIN_METADATA(SelectToSpeakSpeedView)
END_METADATA

}  // namespace ash
