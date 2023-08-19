// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_network_view_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_view.h"
#include "ash/system/network/network_list_mobile_header_view_impl.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_wifi_header_view_impl.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/notreached.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {
using chromeos::network_config::mojom::InhibitReason;

constexpr auto kMainContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);
constexpr auto kTopContainerBorder = gfx::Insets::TLBR(4, 0, 4, 4);

// The following getter methods should only be used for `NetworkType::kWiFi` and
// `NetworkType::kMobile` types otherwise a crash will occur.
std::u16string GetLabelForWifiAndMobileNetwork(NetworkType type) {
  switch (type) {
    case NetworkType::kWiFi:
      return l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_SETTINGS_JOIN_WIFI_NETWORK);
    case NetworkType::kMobile:
      return l10n_util::GetStringUTF16(IDS_ASH_QUICK_SETTINGS_ADD_ESIM);
    default:
      NOTREACHED_NORETURN();
  }
}

std::u16string GetTooltipForWifiAndMobileNetwork(NetworkType type) {
  switch (type) {
    case NetworkType::kWiFi:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_OTHER_WIFI);
    case NetworkType::kMobile:
      return l10n_util::GetStringUTF16(GetAddESimTooltipMessageId());
    default:
      NOTREACHED_NORETURN();
  }
}

int GetViewIDForWifiAndMobileNetwork(NetworkType type) {
  switch (type) {
    case NetworkType::kWiFi:
      return VIEW_ID_JOIN_WIFI_NETWORK_ENTRY;
    case NetworkType::kMobile:
      return VIEW_ID_ADD_ESIM_ENTRY;
    default:
      NOTREACHED_NORETURN();
  }
}
}  // namespace

NetworkDetailedNetworkViewImpl::NetworkDetailedNetworkViewImpl(
    DetailedViewDelegate* detailed_view_delegate,
    NetworkDetailedNetworkView::Delegate* delegate)
    : NetworkDetailedView(detailed_view_delegate,
                          delegate,
                          NetworkDetailedViewListType::LIST_TYPE_NETWORK),
      NetworkDetailedNetworkView(delegate) {
  RecordDetailedViewSection(DetailedViewSection::kDetailedSection);
}

NetworkDetailedNetworkViewImpl::~NetworkDetailedNetworkViewImpl() = default;

void NetworkDetailedNetworkViewImpl::NotifyNetworkListChanged() {
  scroll_content()->InvalidateLayout();
  Layout();

  if (!settings_button()) {
    return;
  }

  if (Shell::Get()->session_controller()->login_status() ==
      LoginStatus::NOT_LOGGED_IN) {
    // When not logged in, only enable the settings button if there is a
    // default (i.e. connected or connecting) network to show settings for.
    settings_button()->SetEnabled(model()->default_network());
  } else {
    // Otherwise, enable if showing settings is allowed. There are situations
    // (supervised user creation flow) when the session is started but UI flow
    // continues within login UI, i.e., no browser window is yet available.
    settings_button()->SetEnabled(
        Shell::Get()->session_controller()->ShouldEnableSettings());
  }
}

views::View* NetworkDetailedNetworkViewImpl::GetAsView() {
  return this;
}

NetworkListNetworkItemView* NetworkDetailedNetworkViewImpl::AddNetworkListItem(
    NetworkType type) {
  return GetNetworkList(type)->AddChildView(
      std::make_unique<NetworkListNetworkItemView>(/*listener=*/this));
}

HoverHighlightView* NetworkDetailedNetworkViewImpl::AddConfigureNetworkEntry(
    NetworkType type) {
  CHECK(type == NetworkType::kWiFi || type == NetworkType::kMobile);
  HoverHighlightView* entry = GetNetworkList(type)->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  entry->SetID(GetViewIDForWifiAndMobileNetwork(type));
  entry->SetTooltipText(GetTooltipForWifiAndMobileNetwork(type));

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemMenuPlusIcon, cros_tokens::kCrosSysPrimary));
  entry->AddViewAndLabel(std::move(image_view),
                         GetLabelForWifiAndMobileNetwork(type));
  views::Label* label = entry->text_label();
  label->SetEnabledColorId(cros_tokens::kCrosSysPrimary);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                        *label);

  return entry;
}

NetworkListWifiHeaderView*
NetworkDetailedNetworkViewImpl::AddWifiSectionHeader() {
  if (!wifi_top_container_ && features::IsQsRevampEnabled()) {
    wifi_top_container_ =
        scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
            RoundedContainer::Behavior::kTopRounded));
    wifi_top_container_->SetBorderInsets(kTopContainerBorder);
    wifi_top_container_->SetProperty(views::kMarginsKey,
                                     gfx::Insets::TLBR(6, 0, 0, 0));
  }
  return (features::IsQsRevampEnabled() ? wifi_top_container_.get()
                                        : scroll_content())
      ->AddChildView(
          std::make_unique<NetworkListWifiHeaderViewImpl>(/*delegate=*/this));
}

NetworkListMobileHeaderView*
NetworkDetailedNetworkViewImpl::AddMobileSectionHeader() {
  if (!mobile_top_container_ && features::IsQsRevampEnabled()) {
    mobile_top_container_ =
        scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
            RoundedContainer::Behavior::kTopRounded));
    mobile_top_container_->SetBorderInsets(kTopContainerBorder);
  }
  return (features::IsQsRevampEnabled() ? mobile_top_container_.get()
                                        : scroll_content())
      ->AddChildView(
          std::make_unique<NetworkListMobileHeaderViewImpl>(/*delegate=*/this));
}

views::View* NetworkDetailedNetworkViewImpl::GetNetworkList(NetworkType type) {
  if (!features::IsQsRevampEnabled()) {
    return scroll_content();
  }

  switch (type) {
    case NetworkType::kWiFi:
      if (!wifi_network_list_view_) {
        wifi_network_list_view_ =
            scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
                RoundedContainer::Behavior::kBottomRounded));

        // Add a small empty space, like a separator, between the containers.
        wifi_network_list_view_->SetProperty(views::kMarginsKey,
                                             kMainContainerMargins);
      }
      return wifi_network_list_view_;
    case NetworkType::kMobile:
    case NetworkType::kTether:
    case NetworkType::kCellular:
      if (!mobile_network_list_view_) {
        mobile_network_list_view_ =
            scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
                RoundedContainer::Behavior::kBottomRounded));

        // Add a small empty space, like a separator, between the containers.
        mobile_network_list_view_->SetProperty(views::kMarginsKey,
                                               kMainContainerMargins);
      }
      return mobile_network_list_view_;
    case NetworkType::kEthernet:
      if (!first_list_view_) {
        first_list_view_ = scroll_content()->AddChildView(
            std::make_unique<RoundedContainer>());
        first_list_view_->SetProperty(views::kMarginsKey,
                                      gfx::Insets::TLBR(0, 0, 6, 0));
      }
      return first_list_view_;
    case NetworkType::kAll:
    default:
      return scroll_content();
  }
  NOTREACHED();
}

void NetworkDetailedNetworkViewImpl::ReorderFirstListView(size_t index) {
  if (first_list_view_) {
    scroll_content()->ReorderChildView(first_list_view_, index);
  }
}

void NetworkDetailedNetworkViewImpl::ReorderNetworkTopContainer(size_t index) {
  if (wifi_top_container_) {
    scroll_content()->ReorderChildView(wifi_top_container_, index);
  }
}

void NetworkDetailedNetworkViewImpl::ReorderNetworkListView(size_t index) {
  if (wifi_network_list_view_) {
    scroll_content()->ReorderChildView(wifi_network_list_view_, index);
  }
}

void NetworkDetailedNetworkViewImpl::ReorderMobileTopContainer(size_t index) {
  if (mobile_top_container_) {
    scroll_content()->ReorderChildView(mobile_top_container_, index);
  }
}

void NetworkDetailedNetworkViewImpl::ReorderMobileListView(size_t index) {
  if (mobile_network_list_view_) {
    scroll_content()->ReorderChildView(mobile_network_list_view_, index);
  }
}

void NetworkDetailedNetworkViewImpl::MaybeRemoveFirstListView() {
  if (first_list_view_ && first_list_view_->children().empty()) {
    scroll_content()->RemoveChildViewT(first_list_view_.get());
    first_list_view_ = nullptr;
  }
}

void NetworkDetailedNetworkViewImpl::UpdateWifiStatus(bool enabled) {
  if (!features::IsQsRevampEnabled()) {
    return;
  }

  if (wifi_top_container_) {
    wifi_top_container_->SetBehavior(
        enabled ? RoundedContainer::Behavior::kTopRounded
                : RoundedContainer::Behavior::kAllRounded);
  }
  if (wifi_network_list_view_) {
    wifi_network_list_view_->SetVisible(enabled);
  }
}

void NetworkDetailedNetworkViewImpl::UpdateMobileStatus(bool enabled) {
  if (!features::IsQsRevampEnabled()) {
    return;
  }

  if (mobile_top_container_) {
    mobile_top_container_->SetBehavior(
        enabled ? RoundedContainer::Behavior::kTopRounded
                : RoundedContainer::Behavior::kAllRounded);
  }
  if (mobile_network_list_view_) {
    mobile_network_list_view_->SetVisible(enabled);
  }
}

void NetworkDetailedNetworkViewImpl::ScrollToPosition(int position) {
  if (GetScrollPosition() == position) {
    return;
  }
  scroller()->ScrollToPosition(scroller()->vertical_scroll_bar(), position);
}

int NetworkDetailedNetworkViewImpl::GetScrollPosition() {
  return scroller()->GetVisibleRect().y();
}

void NetworkDetailedNetworkViewImpl::OnMobileToggleClicked(bool new_state) {
  NetworkDetailedNetworkView::delegate()->OnMobileToggleClicked(new_state);
}

void NetworkDetailedNetworkViewImpl::OnWifiToggleClicked(bool new_state) {
  NetworkDetailedNetworkView::delegate()->OnWifiToggleClicked(new_state);
}

void NetworkDetailedNetworkViewImpl::UpdateScanningBarVisibility(bool visible) {
  ShowProgress(-1, visible);
}

BEGIN_METADATA(NetworkDetailedNetworkViewImpl, views::View)
END_METADATA

}  // namespace ash
