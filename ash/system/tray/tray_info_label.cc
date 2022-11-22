// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_info_label.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

TrayInfoLabel::TrayInfoLabel(int message_id)
    : label_(TrayPopupUtils::CreateDefaultLabel()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/false);
  tri_view->SetInsets(gfx::Insets::TLBR(
      0, kMenuExtraMarginFromLeftEdge + kTrayPopupItemMinStartWidth, 0,
      kTrayPopupPaddingHorizontal));
  tri_view->SetContainerVisible(TriView::Container::START, false);
  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->AddView(TriView::Container::CENTER, label_);

  AddChildView(tri_view);
  SetFocusBehavior(FocusBehavior::NEVER);
  Update(message_id);
}

TrayInfoLabel::~TrayInfoLabel() = default;

void TrayInfoLabel::Update(int message_id) {
  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(label_,
                                   TrayPopupUtils::FontStyle::kSystemInfo);
  label_->SetText(l10n_util::GetStringUTF16(message_id));
}

const char* TrayInfoLabel::GetClassName() const {
  return "TrayInfoLabel";
}

}  // namespace ash
