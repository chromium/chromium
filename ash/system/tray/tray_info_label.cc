// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_info_label.h"

#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

TrayInfoLabel::TrayInfoLabel(int message_id)
    : label_(TrayPopupUtils::CreateDefaultLabel()) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  // TODO(b/330548312): Wrap label properly without workaround
  label_->SetMultiLine(true);
  label_->SetMaximumWidth(kTrayMenuWidth - kMenuExtraMarginFromLeftEdge -
                          kWideTrayPopupItemMinStartWidth -
                          kTrayPopupPaddingHorizontal);

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/false);
  tri_view->SetInsets(gfx::Insets::TLBR(/*top=*/0,
                                        /*left=*/kTrayPopupPaddingHorizontal,
                                        /*bottom=*/0,
                                        /*right=*/kTrayPopupPaddingHorizontal));
  tri_view->SetContainerVisible(TriView::Container::START, false);
  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->AddView(TriView::Container::CENTER, label_);

  AddChildView(tri_view);
  SetFocusBehavior(FocusBehavior::NEVER);
  Update(message_id);
}

TrayInfoLabel::~TrayInfoLabel() = default;

void TrayInfoLabel::Update(int message_id) {
  label_->SetEnabledColorId(kColorAshTextColorPrimary);
  label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label_);
  label_->SetText(l10n_util::GetStringUTF16(message_id));
}

BEGIN_METADATA(TrayInfoLabel)
END_METADATA

}  // namespace ash
