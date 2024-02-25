// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-internet-detail-menu' is a menu that provides
 * additional actions for a network in the network detail page.
 */
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {ESimManagerListenerMixin} from 'chrome://resources/ash/common/cellular_setup/esim_manager_listener_mixin.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './internet_detail_menu.html.js';

export interface SettingsInternetDetailMenuElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

const SettingsInternetDetailMenuElementBase = ESimManagerListenerMixin(
    DeepLinkingMixin(RouteObserverMixin(PolymerElement)));

export class SettingsInternetDetailMenuElement extends
    SettingsInternetDetailMenuElementBase {
  static get is() {
    return 'settings-internet-detail-menu' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Device state for the network type.
       */
      deviceState: Object,

      /**
       * Null if current network on network detail page is not an eSIM network.
       */
      eSimNetworkState_: {
        type: Object,
        value: null,
      },

      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      guid_: {
        type: String,
        value: '',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kCellularRenameESimNetwork,
          Setting.kCellularRemoveESimNetwork,
        ]),
      },
    };
  }

  deviceState: OncMojo.DeviceStateProperties|undefined;
  private eSimNetworkState_: OncMojo.NetworkStateProperties|null;
  private isGuest_: boolean;
  private guid_: string;

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    afterNextRender(this, () => {
      const menu = this.$.menu.get();
      const menuTarget =
          castExists(this.shadowRoot!.getElementById('moreNetworkDetail'));
      menu.showAt(menuTarget);

      // Wait for menu to open.
      afterNextRender(this, () => {
        let element: HTMLElement|null = null;
        if (settingId === Setting.kCellularRenameESimNetwork) {
          element = this.shadowRoot!.getElementById('renameBtn');
        } else {
          element = this.shadowRoot!.getElementById('removeBtn');
        }

        if (!element) {
          console.warn('Deep link element could not be found');
          return;
        }

        this.showDeepLinkElement(element);
        return;
      });
    });

    // Stop deep link attempt since we completed it manually.
    return false;
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(route: Route): void {
    this.eSimNetworkState_ = null;
    this.guid_ = '';
    if (route !== routes.NETWORK_DETAIL) {
      return;
    }

    // Check if the current network is Cellular using the GUID in the
    // current route. We can't use the 'type' parameter in the url
    // directly because Cellular and Tethering share the same subpage and have
    // the same 'type' in the route.
    const queryParams = Router.getInstance().getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.error('No guid specified for page:' + route);
      return;
    }
    this.guid_ = guid;

    // Needed to set initial eSimNetworkState_.
    this.setEsimNetworkState_();
    this.attemptDeepLink();
  }

  /**
   * ESimManagerListenerBehavior override
   */
  override onProfileChanged(): void {
    this.setEsimNetworkState_();
  }

  /**
   * Gets and sets current eSIM network state.
   */
  private async setEsimNetworkState_(): Promise<void> {
    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    const response = await networkConfig.getNetworkState(this.guid_);
    if (!response.result || response.result.type !== NetworkType.kCellular ||
        !response.result.typeState.cellular!.eid ||
        !response.result.typeState.cellular!.iccid) {
      this.eSimNetworkState_ = null;
      return;
    }
    this.eSimNetworkState_ = response.result;
  }

  private onDotsClick_(e: Event): void {
    const menu = this.$.menu.get();
    menu.showAt(e.target as HTMLElement);
  }

  private shouldShowDotsMenuButton_(): boolean {
    // Not shown in guest mode.
    if (this.isGuest_) {
      return false;
    }

    // Show if |this.eSimNetworkState_| has been fetched. Note that this only
    // occurs if this is a cellular network with an ICCID.
    return !!this.eSimNetworkState_;
  }

  private isDotsMenuButtonDisabled_(): boolean {
    // Managed eSIM networks cannot be renamed or removed by user.
    if (this.eSimNetworkState_ &&
        this.eSimNetworkState_.source === OncSource.kDevicePolicy) {
      return true;
    }

    if (!this.deviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.deviceState);
  }

  private onRenameEsimProfileClick_(): void {
    this.closeMenu_();
    const event = new CustomEvent('show-esim-profile-rename-dialog', {
      bubbles: true,
      composed: true,
      detail: {networkState: this.eSimNetworkState_},
    });
    this.dispatchEvent(event);
  }

  private onRemoveEsimProfileClick_(): void {
    this.closeMenu_();
    const event = new CustomEvent('show-esim-remove-profile-dialog', {
      bubbles: true,
      composed: true,
      detail: {networkState: this.eSimNetworkState_},
    });
    this.dispatchEvent(event);
  }

  private closeMenu_(): void {
    const actionMenu =
        castExists(this.shadowRoot!.querySelector('cr-action-menu'));
    actionMenu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetDetailMenuElement.is]: SettingsInternetDetailMenuElement;
  }
}

customElements.define(
    SettingsInternetDetailMenuElement.is, SettingsInternetDetailMenuElement);
