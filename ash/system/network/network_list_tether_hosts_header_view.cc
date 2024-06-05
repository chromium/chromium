// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_tether_hosts_header_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {

NetworkListTetherHostsHeaderView::NetworkListTetherHostsHeaderView(
    OnExpandedStateToggle callback)
    : RoundedContainer(RoundedContainer::Behavior::kTopRounded),
      callback_(std::move(callback)) {
  DCHECK(base::FeatureList::IsEnabled(features::kInstantHotspotRebrand));
  auto* entry_row = AddChildView(std::make_unique<HoverHighlightView>(this));
  entry_row->SetID(
      static_cast<int>(NetworkListTetherHostsHeaderViewChildId::kEntryRow));
  entry_row->GetViewAccessibility().SetName(u"");
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(ui::ImageModel::FromVectorIcon(
      kUnifiedMenuSignalCellular0Icon, cros_tokens::kCrosSysOnSurface));
  entry_row->AddViewAndLabel(
      std::move(image_view),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_TETHER_HOSTS));
  entry_row->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                             *entry_row->text_label());
  entry_row->SetExpandable(true);

  chevron_ = TrayPopupUtils::CreateMainImageView(
      /*use_wide_layout=*/true);
  chevron_->SetID(
      static_cast<int>(NetworkListTetherHostsHeaderViewChildId::kChevron));
  chevron_->SetImage(ui::ImageModel::FromVectorIcon(
      kChevronUpIcon, cros_tokens::kCrosSysOnSurface));
  entry_row->AddRightView(chevron_);
}

NetworkListTetherHostsHeaderView::~NetworkListTetherHostsHeaderView() = default;

void NetworkListTetherHostsHeaderView::OnViewClicked(views::View* sender) {
  ToggleExpandedState();
}

void NetworkListTetherHostsHeaderView::ToggleExpandedState() {
  is_expanded_ = !is_expanded_;

  SetBehavior(is_expanded_ ? RoundedContainer::Behavior::kTopRounded
                           : RoundedContainer::Behavior::kAllRounded);

  if (is_expanded_) {
    chevron_->SetImage(ui::ImageModel::FromVectorIcon(
        kChevronUpIcon, cros_tokens::kCrosSysOnSurface));
  } else {
    chevron_->SetImage(ui::ImageModel::FromVectorIcon(
        kChevronDownIcon, cros_tokens::kCrosSysOnSurface));
  }

  callback_.Run();
}

BEGIN_METADATA(NetworkListTetherHostsHeaderView)
END_METADATA

}  // namespace ash
