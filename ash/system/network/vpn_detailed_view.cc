// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_util.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

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

constexpr auto kContainerShortMargin = gfx::Insets::TLBR(0, 0, 2, 0);
constexpr auto kContainerTallMargin = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kProviderTriViewInsets = gfx::Insets::TLBR(8, 8, 8, 0);

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
  if (network->type != NetworkType::kVPN) {
    return false;
  }

  const VPNStatePropertiesPtr& vpn = network->type_state->get_vpn();
  if (vpn->type == VpnType::kArc || vpn->type == VpnType::kExtension) {
    return vpn->type == provider->type &&
           vpn->provider_id == provider->provider_id;
  }

  // Internal provider types all match the default internal provider.
  return provider->type == VpnType::kOpenVPN;
}

// crbug/1303306: 'Add VPN' button should be disabled on a locked user session
// or before user login.
bool CanAddVpnButtonBeEnabled(LoginStatus login_status) {
  return login_status != LoginStatus::NOT_LOGGED_IN &&
         login_status != LoginStatus::LOCKED &&
         login_status != LoginStatus::KIOSK_APP;
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

views::ImageView* GetPolicyIndicatorIcon() {
  views::ImageView* policy_indicator_icon =
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false);
  policy_indicator_icon->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemMenuBusinessIcon,
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)));
  policy_indicator_icon->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_ASH_ACCESSIBILITY_FEATURE_MANAGED,
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER)));
  return policy_indicator_icon;
}

// Shows the "add network" dialog for a VPN provider.
void ShowAddVpnDialog(VpnType vpn_provider_type,
                      const std::string& vpn_provider_app_id) {
  if (vpn_provider_type == VpnType::kExtension) {
    base::RecordAction(base::UserMetricsAction("StatusArea_VPN_AddThirdParty"));
    Shell::Get()->system_tray_model()->client()->ShowThirdPartyVpnCreate(
        vpn_provider_app_id);
  } else if (vpn_provider_type == VpnType::kArc) {
    // TODO(lgcheng@) Add UMA status if needed.
    Shell::Get()->system_tray_model()->client()->ShowArcVpnCreate(
        vpn_provider_app_id);
  } else {
    base::RecordAction(base::UserMetricsAction("StatusArea_VPN_AddBuiltIn"));
    Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
        ::onc::network_type::kVPN);
  }
}

// A view for a VPN provider. Derives from views::Button so the entire row is
// clickable.
class VPNListProviderEntry : public views::Button {
  METADATA_HEADER(VPNListProviderEntry, views::Button)

 public:
  VPNListProviderEntry(const VpnProviderPtr& vpn_provider,
                       const std::string& name,
                       bool enabled)
      : vpn_provider_type_(vpn_provider->type),
        vpn_provider_app_id_(vpn_provider->app_id) {
    SetCallback(base::BindRepeating(&VPNListProviderEntry::PerformAction,
                                    base::Unretained(this)));
    TrayPopupUtils::ConfigureRowButtonInkdrop(views::InkDrop::Get(this));
    SetHasInkDropActionOnClick(true);
    // Disable focus on the row because we want the keyboard focus ring to
    // appear on the add network button.
    SetFocusBehavior(FocusBehavior::NEVER);

    // Create a TriView to hold the children.
    SetLayoutManager(std::make_unique<views::FillLayout>());
    TriView* tri_view =
        TrayPopupUtils::CreateDefaultRowView(/*use_wide_layout=*/true);
    tri_view->SetInsets(kProviderTriViewInsets);
    tri_view->SetContainerVisible(TriView::Container::START, false);
    AddChildView(tri_view);

    // Add the VPN label with the provider name.
    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(base::UTF8ToUTF16(name));
    label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label);

    tri_view->AddView(TriView::Container::CENTER, label);

    // Add the VPN policy indicator if this provider is disabled.
    if (!enabled) {
      tri_view->AddView(TriView::Container::END, GetPolicyIndicatorIcon());
    }

    // Add the VPN add button.
    auto add_vpn_button = std::make_unique<SystemMenuButton>(
        base::BindRepeating(&ShowAddVpnDialog, vpn_provider_type_,
                            vpn_provider_app_id_),
        gfx::ImageSkia(), gfx::ImageSkia(), IDS_ASH_STATUS_TRAY_ADD_CONNECTION);
    add_vpn_button->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kSystemMenuPlusIcon,
            static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)));
    add_vpn_button->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::ImageModel::FromVectorIcon(
            kSystemMenuPlusIcon,
            static_cast<ui::ColorId>(cros_tokens::kCrosSysDisabled)));

    // The tooltip says "Add connection" which is fine for users who can see the
    // label with the provider name, but ChromeVox users need to hear the name.
    add_vpn_button->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_ADD_VPN_CONNECTION_WITH, base::UTF8ToUTF16(name)));

    // Update enabled state for the whole row and the button.
    bool add_vpn_enabled =
        enabled && CanAddVpnButtonBeEnabled(
                       Shell::Get()->session_controller()->login_status());
    SetEnabled(add_vpn_enabled);
    add_vpn_button->SetEnabled(add_vpn_enabled);

    tri_view->AddView(TriView::Container::END, add_vpn_button.release());
  }

  void PerformAction() {
    ShowAddVpnDialog(vpn_provider_type_, vpn_provider_app_id_);
  }

 private:
  VpnType vpn_provider_type_;
  std::string vpn_provider_app_id_;
};

BEGIN_METADATA(VPNListProviderEntry)
END_METADATA

// A list entry that represents a network. If the network is currently
// connecting, the icon shown by this list entry will be animated. If the
// network is currently connected, a disconnect button will be shown next to its
// name.
class VPNListNetworkEntry : public HoverHighlightView,
                            public network_icon::AnimationObserver {
  METADATA_HEADER(VPNListNetworkEntry, HoverHighlightView)

 public:
  VPNListNetworkEntry(VpnDetailedView* vpn_detailed_view,
                      TrayNetworkStateModel* model,
                      const NetworkStateProperties* network);

  VPNListNetworkEntry(const VPNListNetworkEntry&) = delete;
  VPNListNetworkEntry& operator=(const VPNListNetworkEntry&) = delete;

  ~VPNListNetworkEntry() override;

  // network_icon::AnimationObserver:
  void NetworkIconChanged() override;

 private:
  void OnGetNetworkState(NetworkStatePropertiesPtr result);
  void UpdateFromNetworkState(const NetworkStateProperties* network);

  raw_ptr<TrayNetworkStateModel> model_;
  const std::string guid_;

  base::WeakPtrFactory<VPNListNetworkEntry> weak_ptr_factory_{this};
};

BEGIN_METADATA(VPNListNetworkEntry)
END_METADATA

VPNListNetworkEntry::VPNListNetworkEntry(VpnDetailedView* owner,
                                         TrayNetworkStateModel* model,
                                         const NetworkStateProperties* network)
    : HoverHighlightView(owner), model_(model), guid_(network->guid) {
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

void VPNListNetworkEntry::OnGetNetworkState(NetworkStatePropertiesPtr result) {
  UpdateFromNetworkState(result.get());
}

void VPNListNetworkEntry::UpdateFromNetworkState(
    const NetworkStateProperties* vpn) {
  if (vpn && vpn->connection_state == ConnectionStateType::kConnecting) {
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  if (!vpn) {
    // This is a transient state where the vpn has been removed already but
    // the network list in the UI has not been updated yet.
    return;
  }
  Reset();

  gfx::ImageSkia image = network_icon::GetImageForVPN(
      GetColorProvider(), vpn, network_icon::ICON_TYPE_LIST);
  std::u16string label = network_icon::GetLabelForNetworkList(vpn);
  AddIconAndLabel(image, label);
  if (chromeos::network_config::StateIsConnected(vpn->connection_state)) {
    SetupConnectedScrollListItem(this);
    if (IsVpnConfigAllowed()) {
      auto disconnect_button = std::make_unique<PillButton>(
          // TODO(stevenjb): Replace with mojo API. https://crbug.com/862420.
          base::BindRepeating(&NetworkConnect::DisconnectFromNetworkId,
                              base::Unretained(NetworkConnect::Get()), guid_),
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_VPN_DISCONNECT),
          PillButton::kPrimaryWithoutIcon);
      disconnect_button->GetViewAccessibility().SetName(
          l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECT_BUTTON_A11Y_LABEL, label));
      AddRightView(disconnect_button.release());
    }
    tri_view()->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(gfx::Insets::TLBR(
            0, kTrayPopupButtonEndMargin - kTrayPopupLabelHorizontalPadding, 0,
            kTrayPopupButtonEndMargin)));
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN_WITH_CONNECTION_STATUS,
        label,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED)));
  } else if (vpn->connection_state == ConnectionStateType::kConnecting) {
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN_WITH_CONNECTION_STATUS,
        label,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING)));
    SetupConnectingScrollListItem(this);
  } else {
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT, label));
  }

  DeprecatedLayoutImmediately();
}

}  // namespace

VpnDetailedView::VpnDetailedView(DetailedViewDelegate* delegate,
                                 LoginStatus login)
    : NetworkStateListDetailedView(delegate, LIST_TYPE_VPN, login) {
  model()->vpn_list()->AddObserver(this);
}

VpnDetailedView::~VpnDetailedView() {
  model()->vpn_list()->RemoveObserver(this);
}

void VpnDetailedView::UpdateNetworkList() {
  model()->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kVPN,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&VpnDetailedView::OnGetNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VpnDetailedView::OnGetNetworkStateList(NetworkStateList networks) {
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
  scroll_content()->RemoveAllChildViews();
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
  scroller()->DeprecatedLayoutImmediately();

  if (scroll_to_show_view) {
    // Scroll the list so that `scroll_to_show_view` is in view. The ScrollView
    // may be several layers up the view hierarchy. `ScrollViewToVisible`
    // handles this case.
    const_cast<views::View*>(scroll_to_show_view)->ScrollViewToVisible();
  }
}

bool VpnDetailedView::IsNetworkEntry(views::View* view,
                                     std::string* guid) const {
  const auto& entry = network_view_guid_map_.find(view);
  if (entry == network_view_guid_map_.end()) {
    return false;
  }
  *guid = entry->second;
  return true;
}

void VpnDetailedView::OnVpnProvidersChanged() {
  UpdateNetworkList();
}

void VpnDetailedView::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kVpnConfigAllowed, true);
}

void VpnDetailedView::AddNetwork(const NetworkStateProperties* network,
                                 views::View* container) {
  views::View* entry(new VPNListNetworkEntry(this, model(), network));
  container->AddChildView(entry);
  network_view_guid_map_[entry] = network->guid;
  list_empty_ = false;
}

void VpnDetailedView::AddUnnestedNetwork(
    const NetworkStateProperties* network) {
  views::View* container;
  container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());
  container->SetProperty(views::kMarginsKey, kContainerTallMargin);
  AddNetwork(network, container);
}

void VpnDetailedView::AddProviderAndNetworks(VpnProviderPtr vpn_provider,
                                             const NetworkStateList& networks) {
  std::string vpn_name =
      vpn_provider->type == VpnType::kOpenVPN
          ? l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_VPN_BUILT_IN_PROVIDER)
          : vpn_provider->provider_name;

  // Note: Currently only built-in VPNs can be disabled by policy.
  bool vpn_enabled = vpn_provider->type != VpnType::kOpenVPN ||
                     !model()->IsBuiltinVpnProhibited();

  // Compute whether this provider has any networks configured.
  bool has_networks = false;
  if (vpn_enabled) {
    for (const auto& network : networks) {
      if (VpnProviderMatchesNetwork(vpn_provider.get(), network.get())) {
        has_networks = true;
        break;
      }
    }
  }

  // Set up the container for the provider entry.
  views::View* provider_container;
  // Use square corners on the bottom if the provider has networks.
  RoundedContainer* rounded_container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          has_networks ? RoundedContainer::Behavior::kTopRounded
                       : RoundedContainer::Behavior::kAllRounded));
  // Ensure the provider view ink drop fills the whole container.
  rounded_container->SetBorderInsets(gfx::Insets());
  rounded_container->SetProperty(
      views::kMarginsKey,
      has_networks ? kContainerShortMargin : kContainerTallMargin);
  provider_container = rounded_container;

  // Add a list entry for the VPN provider.
  std::unique_ptr<views::View> provider_view;
  provider_view = std::make_unique<VPNListProviderEntry>(vpn_provider, vpn_name,
                                                         vpn_enabled);
  const VpnProvider* vpn_providerp = vpn_provider.get();
  provider_view_map_[provider_view.get()] = std::move(vpn_provider);
  provider_container->AddChildView(std::move(provider_view));
  list_empty_ = false;

  if (vpn_enabled && has_networks) {
    // Set up the container for the networks.
    views::View* networks_container;
    networks_container =
        scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
            RoundedContainer::Behavior::kBottomRounded));
    networks_container->SetProperty(views::kMarginsKey, kContainerTallMargin);

    // Add the networks belonging to this provider, in the priority order
    // returned by shill.
    for (const auto& network : networks) {
      if (VpnProviderMatchesNetwork(vpn_providerp, network.get())) {
        AddNetwork(network.get(), networks_container);
      }
    }
  }
}

bool VpnDetailedView::ProcessProviderForNetwork(
    const NetworkStateProperties* network,
    const NetworkStateList& networks,
    std::vector<VpnProviderPtr>* providers) {
  for (auto provider_iter = providers->begin();
       provider_iter != providers->end(); ++provider_iter) {
    if (!VpnProviderMatchesNetwork(provider_iter->get(), network)) {
      continue;
    }
    AddProviderAndNetworks(std::move(*provider_iter), networks);
    providers->erase(provider_iter);
    return true;
  }
  return false;
}

void VpnDetailedView::AddProvidersAndNetworks(
    const NetworkStateList& networks) {
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
    if (network->connection_state == ConnectionStateType::kNotConnected) {
      break;
    }
    if (network->type_state->get_vpn()->type != VpnType::kArc) {
      continue;
    }

    // If no matched provider found for this network. Show it unnested.
    // TODO(lgcheng@) add UMA status to track this.
    if (!ProcessProviderForNetwork(network.get(), networks, &arc_providers)) {
      AddUnnestedNetwork(network.get());
    }
  }

  // Add providers with at least one configured network along with their
  // networks. Providers are added in the order of their highest priority
  // network.
  for (const auto& network : networks) {
    ProcessProviderForNetwork(network.get(), networks, &extension_providers);
  }

  // Add providers without any configured networks, in the order that the
  // providers were returned by the extensions system.
  for (VpnProviderPtr& extension_provider : extension_providers) {
    AddProviderAndNetworks(std::move(extension_provider), {});
  }

  // Add Arc VPN providers without any connected or connecting networks. These
  // providers are sorted by last launch time.
  for (VpnProviderPtr& arc_provider : arc_providers) {
    AddProviderAndNetworks(std::move(arc_provider), {});
  }
}

BEGIN_METADATA(VpnDetailedView)
END_METADATA

}  // namespace ash
