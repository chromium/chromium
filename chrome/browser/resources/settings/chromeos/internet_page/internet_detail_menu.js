// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-internet-detail-menu' is a menu that provides
 * additional actions for a network in the network detail page.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';

import {ESimManagerListenerBehavior, ESimManagerListenerBehaviorInterface} from 'chrome://resources/ash/common/cellular_setup/esim_manager_listener_behavior.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';


// TODO(crbug.com/1093185): Implement DeepLinkingBehavior and override methods
// to show the actions for search result.
/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {ESimManagerListenerBehaviorInterface}
 * @implements {DeepLinkingBehaviorInterface}
 */
const SettingsInternetDetailMenuElementBase = mixinBehaviors(
    [RouteObserverBehavior, ESimManagerListenerBehavior, DeepLinkingBehavior],
    PolymerElement);

/** @polymer */
class SettingsInternetDetailMenuElement extends
    SettingsInternetDetailMenuElementBase {
  static get is() {
    return 'settings-internet-detail-menu';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Device state for the network type.
       * @type {!OncMojo.DeviceStateProperties|undefined}
       */
      deviceState: Object,

      /**
       * Null if current network on network detail page is not an eSIM network.
       * @private {?OncMojo.NetworkStateProperties}
       */
      eSimNetworkState_: {
        type: Object,
        value: null,
      },

      /** @private */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      /** @private*/
      guid_: {
        type: String,
        value: '',
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kCellularRenameESimNetwork,
          Setting.kCellularRemoveESimNetwork,
        ]),
      },
    };
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    afterNextRender(this, () => {
      const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
      menu.showAt(/** @type {!HTMLElement} */ (
          this.shadowRoot.querySelector('#moreNetworkDetail')));

      // Wait for menu to open.
      afterNextRender(this, () => {
        let element;
        if (settingId === Setting.kCellularRenameESimNetwork) {
          element = this.shadowRoot.querySelector('#renameBtn');
        } else {
          element = this.shadowRoot.querySelector('#removeBtn');
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
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
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
    this.setESimNetworkState_();
    this.attemptDeepLink();
  }

  /**
   * ESimManagerListenerBehavior override
   * @param {!ESimProfileRemote} profile
   */
  onProfileChanged(profile) {
    this.setESimNetworkState_();
  }

  /**
   * Gets and sets current eSIM network state.
   * @private
   */
  setESimNetworkState_() {
    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    networkConfig.getNetworkState(this.guid_).then(response => {
      if (!response.result || response.result.type !== NetworkType.kCellular ||
          !response.result.typeState.cellular.eid ||
          !response.result.typeState.cellular.iccid) {
        this.eSimNetworkState_ = null;
        return;
      }
      this.eSimNetworkState_ = response.result;
    });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onDotsClick_(e) {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    menu.showAt(/** @type {!HTMLElement} */ (e.target));
  }

  /**
   * @returns {boolean}
   * @private
   */
  shouldShowDotsMenuButton_() {
    // Not shown in guest mode.
    if (this.isGuest_) {
      return false;
    }

    // Show if |this.eSimNetworkState_| has been fetched. Note that this only
    // occurs if this is a cellular network with an ICCID.
    return !!this.eSimNetworkState_;
  }

  /**
   * @return {boolean}
   * @private
   */
  isDotsMenuButtonDisabled_() {
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

  /**
   * @param {!Event} e
   * @private
   */
  onRenameESimProfileTap_(e) {
    this.closeMenu_();
    const event = new CustomEvent('show-esim-profile-rename-dialog', {
      bubbles: true,
      composed: true,
      detail: {networkState: this.eSimNetworkState_},
    });
    this.dispatchEvent(event);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveESimProfileTap_(e) {
    this.closeMenu_();
    const event = new CustomEvent('show-esim-remove-profile-dialog', {
      bubbles: true,
      composed: true,
      detail: {networkState: this.eSimNetworkState_},
    });
    this.dispatchEvent(event);
  }

  /** @private */
  closeMenu_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (
            this.shadowRoot.querySelector('cr-action-menu'));
    actionMenu.close();
  }
}

customElements.define(
    SettingsInternetDetailMenuElement.is, SettingsInternetDetailMenuElement);
