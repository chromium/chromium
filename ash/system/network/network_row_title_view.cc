// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_row_title_view.h"

#include <string>

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

const int kLineHeight = 20;

}  // namespace

NetworkRowTitleView::NetworkRowTitleView(int title_message_id)
    : title_(TrayPopupUtils::CreateDefaultLabel()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  title_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(title_,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  title_->SetLineHeight(kLineHeight);
  title_->SetText(l10n_util::GetStringUTF16(title_message_id));
  AddChildView(title_);
}

NetworkRowTitleView::~NetworkRowTitleView() = default;

const char* NetworkRowTitleView::GetClassName() const {
  return "NetworkRowTitleView";
}

}  // namespace ash
