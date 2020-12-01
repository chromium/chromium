// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_speed_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// User-selectable speech rates.
const std::vector<double> kSpeechRates = {0.5, 1.0, 1.2, 1.6, 2.0};

// View ID for the option that represents the system default speech rate.
constexpr int kDefaultSpeedId = 100;

// Start offset in pixels to use for option views.
constexpr int kOptionInset = 16;

}  // namespace

SelectToSpeakSpeedView::SelectToSpeakSpeedView(Delegate* delegate,
                                               double initial_speech_rate)
    : delegate_(delegate) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  bool any_selected = false;
  for (unsigned int i = 0; i < kSpeechRates.size(); i++) {
    double option_speed = kSpeechRates[i];
    bool is_selected = option_speed == initial_speech_rate;
    // Add 1 to the index, because view IDs cannot be 0.
    auto label = base::ASCIIToUTF16(base::StringPrintf("%.1fx", option_speed));
    AddMenuItem(/*option_id=*/i + 1, label, is_selected);
    any_selected |= is_selected;
  }
  if (!any_selected) {
    default_speech_rate_ = initial_speech_rate;
    auto label = l10n_util::GetStringFUTF16(
        IDS_ASH_SELECT_TO_SPEAK_DEFAULT_OPTION,
        base::ASCIIToUTF16(base::StringPrintf("%.1f", initial_speech_rate)));
    AddMenuItem(/*option_id=*/kDefaultSpeedId, label, /*is_selected=*/true);
  }
}

void SelectToSpeakSpeedView::AddMenuItem(int option_id,
                                         const base::string16& label,
                                         bool is_selected) {
  HoverHighlightView* item = new HoverHighlightView(this);
  AddChildView(item);
  item->AddLabelRow(label, kOptionInset);
  item->SetID(option_id);
  TrayPopupUtils::InitializeAsCheckableRow(item, is_selected, false);
}

void SelectToSpeakSpeedView::OnViewClicked(views::View* sender) {
  unsigned int speed_index = sender->GetID() - 1;
  double selected_rate = default_speech_rate_;
  if (speed_index >= 0 && speed_index < kSpeechRates.size())
    selected_rate = kSpeechRates[speed_index];
  if (selected_rate != 0.0)
    delegate_->OnSpeechRateSelected(selected_rate);
}

BEGIN_METADATA(SelectToSpeakSpeedView, views::BoxLayoutView)
END_METADATA

}  // namespace ash