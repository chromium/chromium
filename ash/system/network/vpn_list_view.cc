// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list_view.h"

#include <memory>
#include <vector>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::VpnProvider;
using chromeos::network_config::mojom::VpnProviderPtr;
using chromeos::network_config::mojom::VPNStatePropertiesPtr;
using chromeos::network_config::mojom::VpnType;

namespace ash {
namespace tray {
namespace {

struct CompareArcVpnProviderByLastLaunchTime {
  bool operator()(const VpnProviderPtr& provider1,
                  const VpnProviderPtr& provider2) {
    return provider1->last_launch_time > provider2->last_launch_time;
  }
};

// Indicates whether |network| belongs to this VPN provider.
bool VpnProviderMatchesNetwork(const VpnProvider* provider,
                               const NetworkStateProperties* network) {
  DCHECK(network);
  // Never display non-VPN networks or VPNs with no provider info.
  if (network->type != NetworkType::kVPN)
    return false;

  const VPNStatePropertiesPtr& vpn = network->type_state->get_vpn();
  if (vpn->type == VpnType::kArc || vpn->type == VpnType::kExtension) {
    return vpn->type == provider->type &&
           vpn->provider_id == provider->provider_id;
  }

  // Internal provider types all match the default internal provider.
  return provider->type == VpnType::kOpenVPN;
}

// Returns the PrefService that should be used for kVpnConfigAllowed, which is
// controlled by policy. If multiple users are logged in, the more restrictive
// policy is most likely in the primary user.
PrefService* GetPrefService() {
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  PrefService* prefs = controller->GetPrimaryUserPrefService();
  return prefs ? prefs : controller->GetActivePrefService();
}

bool IsVpnConfigAllowed() {
  PrefService* prefs = GetPrefService();
  DCHECK(prefs);
  return prefs->GetBoolean(prefs::kVpnConfigAllowed);
}

// A list entry that represents a VPN provider.
class VPNListProviderEntry : public views::ButtonListener, public views::View {
 public:
  // Currently the |enabled| flag will be always true for VPN providers other
  // than the built-in VPNs.
  VPNListProviderEntry(const VpnProviderPtr& vpn_provider,
                       bool top_item,
                       const std::string& name,
                       bool enabled,
                       int button_accessible_name_id)
      : vpn_provider_(vpn_provider->Clone()) {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    TriView* tri_view = TrayPopupUtils::CreateSubHeaderRowView(true);
    tri_view->AddView(TriView::Container::START,
                      TrayPopupUtils::CreateMainImageView());
    AddChildView(tri_view);

    // Add the VPN label.
    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
    style.SetupLabel(label);
    label->SetText(base::ASCIIToUTF16(name));
    tri_view->AddView(TriView::Container::CENTER, label);

    // Add the VPN policy indicator if using this |vpn_provider| is disabled.
    if (!enabled) {
      views::ImageView* policy_indicator_icon = GetPolicyIndicatorIcon();
      tri_view->AddView(TriView::Container::END, policy_indicator_icon);
    }

    // Add the VPN add button.
    const SkColor image_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorProminent);

    const gfx::ImageSkia enabled_icon =
        gfx::CreateVectorIcon(kSystemMenuAddConnectionIcon, image_color);
    const gfx::ImageSkia disabled_icon =
        gfx::CreateVectorIcon(kSystemMenuAddConnectionIcon,
                              AshColorProvider::GetDisabledColor(image_color));

    SystemMenuButton* add_vpn_button = new SystemMenuButton(
        this, enabled_icon, disabled_icon, button_accessible_name_id);

    // 'Add VPN' is disabled in the login screen since user configured
    // device-wide VPNs are unsupported.
    LoginStatus login_status =
        Shell::Get()->session_controller()->login_status();
    add_vpn_button->SetEnabled(enabled &&
                               login_status != LoginStatus::NOT_LOGGED_IN);
    tri_view->AddView(TriView::Container::END, add_vpn_button);
  }

  // views::View:
  const char* GetClassName() const override { return "VPNListProviderEntry"; }

 protected:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    // If the user clicks on a provider entry, request that the "add network"
    // dialog for this provider be shown.
    if (vpn_provider_->type == VpnType::kExtension) {
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED);
      Shell::Get()->system_tray_model()->client()->ShowThirdPartyVpnCreate(
          vpn_provider_->app_id);
    } else if (vpn_provider_->type == VpnType::kArc) {
      // TODO(lgcheng@) Add UMA status if needed.
      Shell::Get()->system_tray_model()->client()->ShowArcVpnCreate(
          vpn_provider_->app_id);
    } else {
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED);
      Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
          ::onc::network_type::kVPN);
    }
  }

 private:
  views::ImageView* GetPolicyIndicatorIcon() {
    views::ImageView* policy_indicator_icon =
        TrayPopupUtils::CreateMainImageView();
    policy_indicator_icon->SetImage(gfx::CreateVectorIcon(
        kSystemMenuBusinessIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
    policy_indicator_icon->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_ACCESSIBILITY_FEATURE_MANAGED,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER)));
    return policy_indicator_icon;
  }

  VpnProviderPtr vpn_provider_;

  DISALLOW_COPY_AND_ASSIGN(VPNListProviderEntry);
};

// A list entry that represents a network. If the network is currently
// connecting, the icon shown by this list entry will be animated. If the
// network is currently connected, a disconnect button will be shown next to its
// name.
class VPNListNetworkEntry : public HoverHighlightView,
                            public network_icon::AnimationObserver {
 public:
  VPNListNetworkEntry(VPNListView* vpn_list_view,
                      TrayNetworkStateModel* model,
                      const NetworkStateProperties* network);
  ~VPNListNetworkEntry() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override { return "VPNListNetworkEntry"; }

 private:
  void OnGetNetworkState(NetworkStatePropertiesPtr result);
  void UpdateFromNetworkState(const NetworkStateProperties* network);

  VPNListView* const owner_;
  TrayNetworkStateModel* model_;
  const std::string guid_;

  views::LabelButton* disconnect_button_ = nullptr;

  base::WeakPtrFactory<VPNListNetworkEntry> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VPNListNetworkEntry);
};

VPNListNetworkEntry::VPNListNetworkEntry(VPNListView* owner,
                                         TrayNetworkStateModel* model,
                                         const NetworkStateProperties* network)
    : HoverHighlightView(owner),
      owner_(owner),
      model_(model),
      guid_(network->guid) {
  UpdateFromNetworkState(network);
}

VPNListNetworkEntry::~VPNListNetworkEntry() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void VPNListNetworkEntry::NetworkIconChanged() {
  model_->cros_network_config()->GetNetworkState(
      guid_, base::BindOnce(&VPNListNetworkEntry::OnGetNetworkState,
                            weak_ptr_factory_.GetWeakPtr()));
}

void VPNListNetworkEntry::ButtonPressed(Button* sender,
                                        const ui::Event& event) {
  if (sender != disconnect_button_) {
    HoverHighlightView::ButtonPressed(sender, event);
    return;
  }

  // TODO(stevenjb): Replace with mojo API. https://crbug.com/862420.
  chromeos::NetworkConnect::Get()->DisconnectFromNetworkId(guid_);
}

void VPNListNetworkEntry::OnGetNetworkState(NetworkStatePropertiesPtr result) {
  UpdateFromNetworkState(result.get());
}

void VPNListNetworkEntry::UpdateFromNetworkState(
    const NetworkStateProperties* vpn) {
  if (vpn && vpn->connection_state == ConnectionStateType::kConnecting)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  if (!vpn) {
    // This is a transient state where the vpn has been removed already but
    // the network list in the UI has not been updated yet.
    return;
  }
  Reset();
  disconnect_button_ = nullptr;

  gfx::ImageSkia image =
      network_icon::GetImageForVPN(vpn, network_icon::ICON_TYPE_LIST);
  base::string16 label = network_icon::GetLabelForNetworkList(vpn);
  AddIconAndLabel(image, label);
  if (chromeos::network_config::StateIsConnected(vpn->connection_state)) {
    owner_->SetupConnectedScrollListItem(this);
    if (IsVpnConfigAllowed()) {
      disconnect_button_ = TrayPopupUtils::CreateTrayPopupButton(
          this, l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECT));
      disconnect_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECT_BUTTON_A11Y_LABEL, label));
      AddRightView(disconnect_button_);
    }
    tri_view()->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(
            0, kTrayPopupButtonEndMargin - kTrayPopupLabelHorizontalPadding, 0,
            kTrayPopupButtonEndMargin));
    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN_WITH_CONNECTION_STATUS,
        label,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED)));
  } else if (vpn->connection_state == ConnectionStateType::kConnecting) {
    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN_WITH_CONNECTION_STATUS,
        label,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING)));
    owner_->SetupConnectingScrollListItem(this);
  } else {
    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT, label));
  }

  Layout();
}

}  // namespace

VPNListView::VPNListView(DetailedViewDelegate* delegate, LoginStatus login)
    : NetworkStateListDetailedView(delegate, LIST_TYPE_VPN, login) {
  model()->vpn_list()->AddObserver(this);
}

VPNListView::~VPNListView() {
  model()->vpn_list()->RemoveObserver(this);
}

void VPNListView::UpdateNetworkList() {
  model()->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kVPN,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&VPNListView::OnGetNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VPNListView::OnGetNetworkStateList(NetworkStateList networks) {
  // Before updating the list, determine whether the user was hovering over one
  // of the VPN provider or network entries.
  VpnProviderPtr hovered_provider;
  std::string hovered_network_guid;
  for (const std::pair<const views::View* const, VpnProviderPtr>& entry :
       provider_view_map_) {
    if (entry.first->IsMouseHovered()) {
      hovered_provider = entry.second->Clone();
      break;
    }
  }
  if (!hovered_provider) {
    for (const std::pair<const views::View* const, std::string>& entry :
         network_view_guid_map_) {
      if (entry.first->IsMouseHovered()) {
        hovered_network_guid = entry.second;
        break;
      }
    }
  }

  // Clear the list.
  scroll_content()->RemoveAllChildViews(true);
  provider_view_map_.clear();
  network_view_guid_map_.clear();
  list_empty_ = true;

  // Show all VPN providers and all networks that are currently disconnected.
  AddProvidersAndNetworks(networks);

  // Determine whether one of the new list entries corresponds to the entry that
  // the user was previously hovering over. If such an entry is found, the list
  // will be scrolled to ensure the entry is visible.
  const views::View* scroll_to_show_view = nullptr;
  if (hovered_provider) {
    for (const std::pair<const views::View* const, VpnProviderPtr>& entry :
         provider_view_map_) {
      if (entry.second->Equals(*hovered_provider)) {
        scroll_to_show_view = entry.first;
        break;
      }
    }
  } else if (!hovered_network_guid.empty()) {
    for (const std::pair<const views::View* const, std::string>& entry :
         network_view_guid_map_) {
      if (entry.second == hovered_network_guid) {
        scroll_to_show_view = entry.first;
        break;
      }
    }
  }

  // Layout the updated list.
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();

  if (scroll_to_show_view) {
    // Scroll the list so that |scroll_to_show_view| is in view.
    scroll_content()->ScrollRectToVisible(scroll_to_show_view->bounds());
  }
}

bool VPNListView::IsNetworkEntry(views::View* view, std::string* guid) const {
  const auto& entry = network_view_guid_map_.find(view);
  if (entry == network_view_guid_map_.end())
    return false;
  *guid = entry->second;
  return true;
}

void VPNListView::OnVpnProvidersChanged() {
  UpdateNetworkList();
}

void VPNListView::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kVpnConfigAllowed, true);
}

const char* VPNListView::GetClassName() const {
  return "VPNListView";
}

void VPNListView::AddNetwork(const NetworkStateProperties* network) {
  views::View* entry(new VPNListNetworkEntry(this, model(), network));
  scroll_content()->AddChildView(entry);
  network_view_guid_map_[entry] = network->guid;
  list_empty_ = false;
}

void VPNListView::AddProviderAndNetworks(VpnProviderPtr vpn_provider,
                                         const NetworkStateList& networks) {
  // Add a visual separator, unless this is the topmost entry in the list.
  if (!list_empty_) {
    scroll_content()->AddChildView(CreateListSubHeaderSeparator());
  }
  std::string vpn_name =
      vpn_provider->type == VpnType::kOpenVPN
          ? l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER)
          : vpn_provider->provider_name;

  // Add a list entry for the VPN provider.
  views::View* provider_view = nullptr;

  // Note: Currently only built-in VPNs can be disabled by policy.
  bool vpn_enabled =
      vpn_provider->type != VpnType::kOpenVPN || model()->IsBuiltinVpnEnabled();

  provider_view =
      new VPNListProviderEntry(vpn_provider, list_empty_, vpn_name, vpn_enabled,
                               IDS_ASH_STATUS_TRAY_ADD_CONNECTION);
  scroll_content()->AddChildView(provider_view);
  const VpnProvider* vpn_providerp = vpn_provider.get();
  provider_view_map_[provider_view] = std::move(vpn_provider);
  list_empty_ = false;

  if (vpn_enabled) {
    // Add the networks belonging to this provider, in the priority order
    // returned by shill.
    for (const auto& network : networks) {
      if (VpnProviderMatchesNetwork(vpn_providerp, network.get()))
        AddNetwork(network.get());
    }
  }
}

bool VPNListView::ProcessProviderForNetwork(
    const NetworkStateProperties* network,
    const NetworkStateList& networks,
    std::vector<VpnProviderPtr>* providers) {
  for (auto provider_iter = providers->begin();
       provider_iter != providers->end(); ++provider_iter) {
    if (!VpnProviderMatchesNetwork(provider_iter->get(), network))
      continue;
    AddProviderAndNetworks(std::move(*provider_iter), networks);
    providers->erase(provider_iter);
    return true;
  }
  return false;
}

void VPNListView::AddProvidersAndNetworks(const NetworkStateList& networks) {
  // Copy the list of Extension VPN providers enabled in the primary user's
  // profile.
  std::vector<VpnProviderPtr> extension_providers;
  for (const VpnProviderPtr& provider :
       model()->vpn_list()->extension_vpn_providers()) {
    extension_providers.push_back(provider->Clone());
  }
  // Copy the list of Arc VPN providers installed in the primary user's profile.
  std::vector<VpnProviderPtr> arc_providers;
  for (const VpnProviderPtr& provider :
       model()->vpn_list()->arc_vpn_providers()) {
    arc_providers.push_back(provider->Clone());
  }

  std::sort(arc_providers.begin(), arc_providers.end(),
            CompareArcVpnProviderByLastLaunchTime());

  // Add connected ARCVPN network. If we can find the correct provider, nest
  // the network under the provider. Otherwise list it unnested.
  for (const auto& network : networks) {
    if (network->connection_state == ConnectionStateType::kNotConnected)
      break;
    if (network->type_state->get_vpn()->type != VpnType::kArc)
      continue;

    // If no matched provider found for this network. Show it unnested.
    // TODO(lgcheng@) add UMA status to track this.
    if (!ProcessProviderForNetwork(network.get(), networks, &arc_providers))
      AddNetwork(network.get());
  }

  // Add providers with at least one configured network along with their
  // networks. Providers are added in the order of their highest priority
  // network.
  for (const auto& network : networks)
    ProcessProviderForNetwork(network.get(), networks, &extension_providers);

  // Add providers without any configured networks, in the order that the
  // providers were returned by the extensions system.
  for (VpnProviderPtr& extension_provider : extension_providers)
    AddProviderAndNetworks(std::move(extension_provider), {});

  // Add Arc VPN providers without any connected or connecting networks. These
  // providers are sorted by last launch time.
  for (VpnProviderPtr& arc_provider : arc_providers) {
    AddProviderAndNetworks(std::move(arc_provider), {});
  }
}

}  // namespace tray
}  // namespace ash
