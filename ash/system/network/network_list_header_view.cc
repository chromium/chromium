// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_header_view.h"

#include <string>

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {
const int kLineHeight = 20;
}  // namespace

NetworkListHeaderView::NetworkListHeaderView(int label_id) {
  TrayPopupUtils::ConfigureAsStickyHeader(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = TrayPopupUtils::CreateSubHeaderRowView(/*start_visible=*/true);
  if (!features::IsQsRevampEnabled()) {
    container_->AddView(
        TriView::Container::START,
        TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false));
  }

  AddChildView(container_);
  AddTitleView(label_id);
}

void NetworkListHeaderView::AddTitleView(int label_id) {
  DCHECK(container_);
  views::Label* const titleLabelView = TrayPopupUtils::CreateDefaultLabel();
  titleLabelView->SetEnabledColor(
      ash::AshColorProvider::Get()->GetContentLayerColor(
          ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(titleLabelView,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  titleLabelView->SetLineHeight(kLineHeight);
  titleLabelView->SetText(l10n_util::GetStringUTF16(label_id));
  titleLabelView->SetID(kTitleLabelViewId);

  container_->AddView(TriView::Container::CENTER, titleLabelView);
}

}  // namespace ash
