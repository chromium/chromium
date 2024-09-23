// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of Cellular network
 * states
 */

import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../os_settings_icons.css.js';
import './esim_install_error_dialog.js';

import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {ESimManagerListenerMixin} from 'chrome://resources/ash/common/cellular_setup/esim_manager_listener_mixin.js';
import {getEuicc} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getSimSlotCount} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkList} from 'chrome://resources/ash/common/network/network_list_types.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {ESimProfileProperties, ESimProfileRemote, EuiccRemote, ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {CrosNetworkConfigInterface, GlobalPolicy, InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {MultiDeviceBrowserProxyImpl} from '../multidevice_page/multidevice_browser_proxy.js';
import {MultiDeviceFeatureState, MultiDevicePageContentData} from '../multidevice_page/multidevice_constants.js';

import {getTemplate} from './cellular_networks_list.html.js';

declare global {
  interface HTMLElementEventMap {
    'install-profile': CustomEvent<{iccid: string}>;
  }
}

const CellularNetworksListElementBase =
    ESimManagerListenerMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class CellularNetworksListElement extends
    CellularNetworksListElementBase {
  static get is() {
    return 'cellular-networks-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The list of network state properties for the items to display.
       */
      networks: {
        type: Array,
        value() {
          return [];
        },
        observer: 'onNetworksListChanged_',
      },

      /**
       * Whether to show technology badge on mobile network icons.
       */
      showTechnologyBadge: Boolean,

      /**
       * Device state for the cellular network type.
       */
      cellularDeviceState: Object,

      isConnectedToNonCellularNetwork: {
        type: Boolean,
      },

      /**
       * If true, inhibited spinner can be shown, it will be shown
       * if true and cellular is inhibited.
       */
      canShowSpinner: {
        type: Boolean,
      },

      /**
       * Device state for the tether network type. This device state should be
       * used for instant tether networks.
       */
      tetherDeviceState: Object,

      globalPolicy: Object,

      /**
       * The list of eSIM network state properties for display.
       */
      eSimNetworks_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Dictionary mapping pending eSIM profile iccids to pending eSIM
       * profiles.
       */
      profilesMap_: {
        type: Object,
        value() {
          return new Map();
        },
      },

      /**
       * The list of pending eSIM profiles to display after the list of eSIM
       * networks.
       */
      eSimPendingProfileItems_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The list of pSIM network state properties for display.
       */
      pSimNetworks_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The list of tether network state properties for display.
       */
      tetherNetworks_: {
        type: Array,
        value() {
          return [];
        },
      },

      shouldShowEidDialog_: {
        type: Boolean,
        value: false,
      },

      shouldShowInstallErrorDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Euicc object representing the active euicc_ module on the device
       */
      euicc_: {
        type: Object,
        value: null,
      },

      /**
       * The current eSIM profile being installed.
       */
      installingESimProfile_: {
        type: Object,
        value: null,
      },

      /**
       * The error code returned when eSIM profile install attempt was made.
       */
      eSimProfileInstallError_: {
        type: Object,
        value: null,
      },

      /**
       * Multi-device page data used to determine if the tether section should
       * be shown or not.
       */
      multiDevicePageContentData_: {
        type: Object,
        value: null,
      },

      isDeviceInhibited_: {
        type: Boolean,
        computed: 'computeIsDeviceInhibited_(cellularDeviceState,' +
            'cellularDeviceState.inhibitReason)',
      },
      /**
       * Return true if instant hotspot rebrand feature flag is enabled
       */
      isInstantHotspotRebrandEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isInstantHotspotRebrandEnabled') &&
              loadTimeData.getBoolean('isInstantHotspotRebrandEnabled');
        },
      },
    };
  }

  canShowSpinner: boolean;
  cellularDeviceState: OncMojo.DeviceStateProperties|undefined;
  globalPolicy: GlobalPolicy|undefined;
  isConnectedToNonCellularNetwork: boolean;
  networks: OncMojo.NetworkStateProperties[];
  showTechnologyBadge: boolean;
  tetherDeviceState: OncMojo.DeviceStateProperties|undefined;

  private eSimPendingProfileItems_: NetworkList.CustomItemState[];
  private eSimProfileInstallError_: ProfileInstallResult|null;
  private eSimNetworks_: OncMojo.NetworkStateProperties[];
  private euicc_: EuiccRemote|null;
  private installingESimProfile_: ESimProfileRemote|null;
  private isDeviceInhibited_: boolean;
  private isInstantHotspotRebrandEnabled_: boolean;
  private multiDevicePageContentData_: MultiDevicePageContentData|null;
  private networkConfig_: CrosNetworkConfigInterface;
  private profilesMap_: Map<string, ESimProfileRemote>;
  private pSimNetworks_: OncMojo.NetworkStateProperties[];
  private shouldShowEidDialog_: boolean;
  private shouldShowInstallErrorDialog_: boolean;
  private tetherNetworks_: OncMojo.NetworkStateProperties[];

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.fetchEuiccAndEsimPendingProfileList_();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('install-profile', this.installProfile_);

    this.addWebUiListener(
        'settings.updateMultidevicePageContentData',
        this.onMultiDevicePageContentDataChanged_.bind(this));

    MultiDeviceBrowserProxyImpl.getInstance().getPageContentData().then(
        this.onMultiDevicePageContentDataChanged_.bind(this));
  }

  override onAvailableEuiccListChanged(): void {
    this.fetchEuiccAndEsimPendingProfileList_();
  }

  private fetchEuiccAndEsimPendingProfileList_(): void {
    getEuicc().then(euicc => {
      if (!euicc) {
        return;
      }
      this.euicc_ = euicc;
    });
  }

  /**
   * Return true if esim section should be shown.
   */
  private shouldShowEsimSection_(): boolean {
    if (!this.cellularDeviceState) {
      return false;
    }
    const {eSimSlots} = getSimSlotCount(this.cellularDeviceState);
    // Check both the SIM slot infos and the number of EUICCs because the former
    // comes from Shill and the latter from Hermes, so there may be instances
    // where one may be true while they other isn't.
    return !!this.euicc_ && eSimSlots > 0;
  }


  private async processEsimPendingProfiles_(profiles: ESimProfileRemote[]):
      Promise<void> {
    this.profilesMap_ = new Map();
    const eSimPendingProfilePromises =
        profiles.map(this.createEsimPendingProfilePromise_.bind(this));
    const eSimPendingProfileItems =
        await Promise.all(eSimPendingProfilePromises);
    this.eSimPendingProfileItems_ = eSimPendingProfileItems;
  }

  private async createEsimPendingProfilePromise_(profile: ESimProfileRemote):
      Promise<NetworkList.CustomItemState> {
    const response = await profile.getProperties();
    this.profilesMap_.set(response.properties.iccid, profile);
    return this.createEsimPendingProfileItem_(response.properties);
  }

  private createEsimPendingProfileItem_(properties: ESimProfileProperties):
      NetworkList.CustomItemState {
    return {
      customItemType: properties.state === ProfileState.kInstalling ?
          NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE :
          NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
      customItemName: mojoString16ToString(properties.name),
      customItemSubtitle: mojoString16ToString(properties.serviceProvider),
      polymerIcon: 'network:cellular-0',
      showBeforeNetworksList: false,
      customData: {
        iccid: properties.iccid,
      },
    };
  }

  private onNetworksListChanged_(): void {
    const pSimNetworks: OncMojo.NetworkStateProperties[] = [];
    const eSimNetworks: OncMojo.NetworkStateProperties[] = [];
    const tetherNetworks: OncMojo.NetworkStateProperties[] = [];

    for (const network of this.networks) {
      if (network.type === NetworkType.kTether) {
        tetherNetworks.push(network);
        continue;
      }

      if (network.typeState.cellular && network.typeState.cellular.eid) {
        eSimNetworks.push(network);
      } else {
        pSimNetworks.push(network);
      }
    }
    this.eSimNetworks_ = eSimNetworks;
    this.pSimNetworks_ = pSimNetworks;
    this.tetherNetworks_ = tetherNetworks;
  }

  private shouldShowNetworkSublist_(
      ...lists: NetworkList.NetworkListItemType[][]): boolean {
    const totalListLength = lists.reduce((accumulator, currentList) => {
      return accumulator + currentList.length;
    }, 0);
    return totalListLength > 0;
  }

  private shouldShowPsimSection_(
      pSimNetworks: OncMojo.NetworkStateProperties[],
      cellularDeviceState: OncMojo.DeviceStateProperties|undefined): boolean {
    const {pSimSlots} = getSimSlotCount(cellularDeviceState);
    if (pSimSlots > 0) {
      return true;
    }
    // Dual MBIM currently doesn't support eSIM hotswap (b/229619768), which
    // leads Hermes to always show two Eids after swap with pSIM. So, we should
    // also check if there's pSimNetworks available to work around this
    // limitation.
    return this.shouldShowNetworkSublist_(pSimNetworks);
  }

  private onMultiDevicePageContentDataChanged_(
      newData: MultiDevicePageContentData): void {
    this.multiDevicePageContentData_ = newData;
  }

  private shouldShowTetherSection_(pageContentData: MultiDevicePageContentData|
                                   null): boolean {
    if (!pageContentData) {
      return false;
    }
    if (this.isInstantHotspotRebrandEnabled_) {
      return false;
    }
    return pageContentData.instantTetheringState ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  }

  private onAddEsimLinkClicked_(event: CustomEvent<{event: Event}>): void {
    event.detail.event.preventDefault();
    event.stopPropagation();

    const showCellularSetupEvent = new CustomEvent('show-cellular-setup', {
      bubbles: true,
      composed: true,
      detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI},
    });
    this.dispatchEvent(showCellularSetupEvent);
  }

  private onEsimDotsClick_(e: Event): void {
    const menu = this.shadowRoot!
                     .querySelector<CrLazyRenderElement<CrActionMenuElement>>(
                         '#menu')!.get();
    menu.showAt(e.target as HTMLElement);
  }

  private onShowEidDialogClick_(): void {
    const actionMenu =
        castExists(this.shadowRoot!.querySelector('cr-action-menu'));
    actionMenu.close();
    this.shouldShowEidDialog_ = true;
  }

  private onCloseEidDialog_(): void {
    this.shouldShowEidDialog_ = false;
  }

  private installProfile_(event: CustomEvent<{iccid: string}>): void {
    if (!this.isConnectedToNonCellularNetwork) {
      const event = new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('eSimNoConnectionErrorToast'),
      });
      this.dispatchEvent(event);
      return;
    }
    this.installingESimProfile_ =
        castExists(this.profilesMap_.get(event.detail.iccid));
    this.installingESimProfile_.installProfile('').then((response) => {
      if (response.result === ProfileInstallResult.kSuccess) {
        this.eSimProfileInstallError_ = null;
        this.installingESimProfile_ = null;
      } else {
        this.eSimProfileInstallError_ = response.result;
        this.showInstallErrorDialog_();
      }
    });
  }

  private showInstallErrorDialog_(): void {
    this.shouldShowInstallErrorDialog_ = true;
  }

  private onCloseInstallErrorDialog_(): void {
    this.shouldShowInstallErrorDialog_ = false;
  }

  private shouldShowAddEsimButton_(cellularDeviceState:
                                       OncMojo.DeviceStateProperties|
                                   undefined): boolean {
    assert(this.euicc_);
    return this.deviceIsEnabled_(cellularDeviceState);
  }

  private isAddEsimButtonDisabled_(
      cellularDeviceState: OncMojo.DeviceStateProperties|undefined,
      globalPolicy: GlobalPolicy): boolean {
    if (this.isDeviceInhibited_) {
      return true;
    }
    if (!this.deviceIsEnabled_(cellularDeviceState)) {
      return true;
    }
    if (!globalPolicy) {
      return false;
    }
    return globalPolicy.allowOnlyPolicyCellularNetworks;
  }

  /**
   * Return true if the policy indicator that next to the add cellular button
   * should be shown. This policy icon indicates the reason of disabling the
   * add cellular button.
   */
  private shouldShowAddEsimPolicyIcon_(globalPolicy: GlobalPolicy): boolean {
    return globalPolicy && globalPolicy.allowOnlyPolicyCellularNetworks;
  }

  private deviceIsEnabled_(cellularDeviceState: OncMojo.DeviceStateProperties|
                           undefined): boolean {
    return !!cellularDeviceState &&
        cellularDeviceState.deviceState === DeviceStateType.kEnabled;
  }

  private computeIsDeviceInhibited_(): boolean {
    if (!this.cellularDeviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.cellularDeviceState);
  }

  private onAddEsimButtonClick_(): void {
    const event = new CustomEvent('show-cellular-setup', {
      bubbles: true,
      composed: true,
      detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI},
    });
    this.dispatchEvent(event);
  }

  /*
   * Returns the add esim button. If the device does not have an EUICC, no eSIM
   * slot, or policies prohibit users from adding a network, null is returned.
   * @return {?HTMLElement}
   */
  getAddEsimButton(): CrIconButtonElement|null {
    return this.shadowRoot!.querySelector<CrIconButtonElement>(
        '#addESimButton');
  }

  private getInhibitedSubtextMessage_(): string {
    if (!this.cellularDeviceState) {
      return '';
    }

    const inhibitReason = this.cellularDeviceState.inhibitReason;

    switch (inhibitReason) {
      case InhibitReason.kInstallingProfile:
        return this.i18n('cellularNetworkInstallingProfile');
      case InhibitReason.kRenamingProfile:
        return this.i18n('cellularNetworkRenamingProfile');
      case InhibitReason.kRemovingProfile:
        return this.i18n('cellularNetworkRemovingProfile');
      case InhibitReason.kConnectingToProfile:
        return this.i18n('cellularNetworkConnectingToProfile');
      case InhibitReason.kRefreshingProfileList:
        return this.i18n('cellularNetworRefreshingProfileListProfile');
      case InhibitReason.kResettingEuiccMemory:
        return this.i18n('cellularNetworkResettingESim');
      case InhibitReason.kRequestingAvailableProfiles:
        return this.i18n('cellularNetworkRequestingAvailableProfiles');
    }

    return '';
  }

  private isInhibitedOrAffectedByPolicy_(): boolean {
    if (this.cellularDeviceState &&
        this.cellularDeviceState.inhibitReason !== undefined &&
        this.cellularDeviceState.inhibitReason !==
            InhibitReason.kNotInhibited) {
      return true;
    }
    return !!this.globalPolicy &&
        this.globalPolicy.allowOnlyPolicyCellularNetworks;
  }

  /**
   * Return true IFF there are no eSIM profiles installed and we are not
   * installing a profile, refreshing the profile list, or requesting available
   * profiles.
   */
  private shouldShowNoEsimNetworksMessage_(): boolean {
    if (this.cellularDeviceState &&
        this.cellularDeviceState.inhibitReason !== undefined) {
      const inhibitReason = this.cellularDeviceState.inhibitReason;
      if (inhibitReason === InhibitReason.kInstallingProfile ||
          inhibitReason === InhibitReason.kRefreshingProfileList ||
          inhibitReason === InhibitReason.kRequestingAvailableProfiles) {
        return false;
      }
    }
    return !this.shouldShowNetworkSublist_(
        this.eSimNetworks_, this.eSimPendingProfileItems_);
  }

  /**
   * Return true IFF there are no eSIM profiles installed, and the cellular
   * device is inhibited for any reason NOT related to changes to the eSIM
   * profile list or policy restricts the user from adding an eSIM profile.
   */
  private shouldShowNoEsimNetworksMessageWithoutLink_(): boolean {
    return this.shouldShowNoEsimNetworksMessage_() &&
        this.isInhibitedOrAffectedByPolicy_();
  }

  /**
   * Return true IFF there are no eSIM profiles installed, and the cellular
   * device is NOT inhibited for any reason related to changes to the eSIM
   * profile list and policy does NOT restrict the user from adding an eSIM
   * profile.
   */
  private shouldShowAddEsimMessageWithLink(): boolean {
    return this.shouldShowNoEsimNetworksMessage_() &&
        !this.isInhibitedOrAffectedByPolicy_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CellularNetworksListElement.is]: CellularNetworksListElement;
  }
}

customElements.define(
    CellularNetworksListElement.is, CellularNetworksListElement);
