// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import './nearby_share_contact_visibility_dialog.js';
import './nearby_share_device_name_dialog.js';
import './nearby_share_data_usage_dialog.js';
import './nearby_share_receive_dialog.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {flush, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {getContactManager} from '../../shared/nearby_contact_manager.js';
import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {NearbyAccountManagerBrowserProxyImpl} from './nearby_account_manager_browser_proxy.js';
import {observeReceiveManager} from './nearby_share_receive_manager.js';
import {dataUsageStringToEnum, NearbyShareDataUsage} from './types.js';

/**
 * @type {!number}
 * @private
 */
const DEFAULT_HIGH_VISIBILITY_TIMEOUT_S = 300;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsNearbyShareSubpageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class SettingsNearbyShareSubpageElement extends
    SettingsNearbyShareSubpageElementBase {
  static get is() {
    return 'settings-nearby-share-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      profileName_: {
        type: String,
        value: '',
      },

      profileLabel_: {
        type: String,
        value: '',
      },

      /** @type {NearbySettings} */
      settings: {
        type: Object,
        notify: true,
        value: {},
      },

      /** @private {boolean} */
      isSettingsRetreived: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showDeviceNameDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showVisibilityDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showDataUsageDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showReceiveDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      manageContactsUrl_: {
        type: String,
        value: () => loadTimeData.getString('nearbyShareManageContactsUrl'),
      },

      /** @private {boolean} */
      inHighVisibility_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kNearbyShareOnOff,
          Setting.kNearbyShareDeviceName,
          Setting.kNearbyShareDeviceVisibility,
          Setting.kNearbyShareContacts,
          Setting.kNearbyShareDataUsage,
          Setting.kDevicesNearbyAreSharingNotificationOnOff,
        ]),
      },

      /** @private */
      shouldShowFastInititationNotificationToggle_: {
        type: Boolean,
        computed: `computeShouldShowFastInititationNotificationToggle_(
                settings.isFastInitiationHardwareSupported)`,
      },
    };
  }

  static get observers() {
    return ['enabledChange_(settings.enabled)'];
  }

  constructor() {
    super();

    /** @private {?nearbyShare.mojom.ReceiveObserverReceiver} */
    this.receiveObserver_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('onboarding-cancelled', this.onOnboardingCancelled_);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // TODO(b/166779043): Check whether the Account Manager is enabled and fall
    // back to profile name, or just hide the row. This is not urgent because
    // the Account Manager should be available whenever Nearby Share is enabled.
    NearbyAccountManagerBrowserProxyImpl.getInstance().getAccounts().then(
        accounts => {
          if (accounts.length === 0) {
            return;
          }

          this.profileName_ = accounts[0].fullName;
          this.profileLabel_ = accounts[0].email;
        });
    this.receiveObserver_ = observeReceiveManager(
        /** @type {!nearbyShare.mojom.ReceiveObserverInterface} */ (this));
  }

  /** @private */
  enabledChange_(newValue, oldValue) {
    if (oldValue === undefined && newValue) {
      // Trigger a contact sync whenever the Nearby subpage is opened and
      // nearby is enabled complete to improve consistency. This should help
      // avoid scenarios where a share is attempted and contacts are stale on
      // the receiver.
      getContactManager().downloadContacts();
    }
  }

  /** @private */
  onDeviceNameTap_() {
    if (this.showDeviceNameDialog_) {
      return;
    }
    this.showDeviceNameDialog_ = true;
  }

  /** @private */
  onVisibilityTap_() {
    this.showVisibilityDialog_ = true;
  }

  /** @private */
  onDataUsageTap_() {
    this.showDataUsageDialog_ = true;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onDeviceNameDialogClose_(event) {
    this.showDeviceNameDialog_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onVisibilityDialogClose_(event) {
    this.showVisibilityDialog_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onDataUsageDialogClose_(event) {
    this.showDataUsageDialog_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onReceiveDialogClose_(event) {
    this.showReceiveDialog_ = false;
    this.inHighVisibility_ = false;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onManageContactsTap_(event) {
    window.open(this.manageContactsUrl_);
  }

  /**
   * @private
   * @return {string} Sublabel for manage contacts row.
   */
  getManageContactsSubLabel_() {
    // Remove the protocol part of the contacts url.
    return this.manageContactsUrl_.replace(/(^\w+:|^)\/\//, '');
  }

  /**
   * Mojo callback when high visibility changes.
   * @param {boolean} inHighVisibility
   */
  onHighVisibilityChanged(inHighVisibility) {
    this.inHighVisibility_ = inHighVisibility;
  }

  /**
   * Mojo callback when transfer status changes.
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @param {!nearbyShare.mojom.TransferMetadata} metadata
   */
  onTransferUpdate(shareTarget, metadata) {
    // Note: Intentionally left empty.
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped() {
    this.inHighVisibility_ = false;
  }

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure() {
    this.inHighVisibility_ = false;
  }

  /** @private */
  onInHighVisibilityToggledByUser_() {
    if (this.inHighVisibility_) {
      this.showHighVisibilityPage_();
    }
  }

  /**
   * @param {boolean} state boolean state that determines which string to show
   * @param {string} onstr string to show when state is true
   * @param {string} offstr string to show when state is false
   * @return {string} localized string
   * @private
   */
  getOnOffString_(state, onstr, offstr) {
    return state ? onstr : offstr;
  }

  /**
   * @param {string} name name of device
   * @return {string} localized string
   * @private
   */
  getEditNameButtonAriaDescription_(name) {
    return this.i18n('nearbyShareDeviceNameAriaDescription', name);
  }

  /**
   * @param {nearbyShare.mojom.Visibility} visibility
   * @return {string} localized visibility string
   * @private
   */
  getVisibilityText_(visibility) {
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      case nearbyShare.mojom.Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknown');
      default:
        return '';  // Make closure happy.
    }
  }

  /**
   * @param {nearbyShare.mojom.Visibility} visibility
   * @return {string} localized visibility description string
   * @private
   */
  getVisibilityDescription_(visibility) {
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAllDescription');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySomeDescription');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNoneDescription');
      case nearbyShare.mojom.Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknownDescription');
      default:
        return '';  // Make closure happy.
    }
  }

  /**
   * @param {boolean} inHighVisibility
   */
  getHighVisibilityToggleText_(inHighVisibility) {
    // TODO(crbug.com/1154830): Add logic to show how much time the user
    // actually has left.
    return inHighVisibility ?
        this.i18n('nearbyShareHighVisibilityOn', 5) :
        this.i18nAdvanced(
            'nearbyShareHighVisibilityOff', {substitutions: ['5']});
  }

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getDataUsageLabel_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataLabel');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineLabel');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyLabel');
    }
  }

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getDataUsageSubLabel_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyDescription');
    }
  }

  /**
   * @param {string} dataUsageValue enum value of data usage setting.
   * @return {string} localized string
   * @private
   */
  getEditDataUsageButtonAriaDescription_(dataUsageValue) {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataEditButtonDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineEditButtonDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyEditButtonDescription');
    }
  }

  /**
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== routes.NEARBY_SHARE) {
      return;
    }

    const router = Router.getInstance();
    const queryParams = router.getQueryParameters();

    if (queryParams.has('deviceName')) {
      this.showDeviceNameDialog_ = true;
    }

    if (queryParams.has('visibility')) {
      this.showVisibilityDialog_ = true;
    }

    if (queryParams.has('receive')) {
      this.showHighVisibilityPage_(Number(queryParams.get('timeout')));
    }

    if (queryParams.has('confirm')) {
      this.showReceiveDialog_ = true;
      flush();
      this.shadowRoot.querySelector('#receiveDialog').showConfirmPage();
    }

    if (queryParams.has('onboarding')) {
      this.showOnboarding_();
    }

    this.attemptDeepLink();
  }

  /**
   * @param {number=} timeoutInSeconds
   * @private
   */
  showHighVisibilityPage_(timeoutInSeconds) {
    const shutoffTimeoutInSeconds =
        timeoutInSeconds || DEFAULT_HIGH_VISIBILITY_TIMEOUT_S;
    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot.querySelector('#receiveDialog')
        .showHighVisibilityPage(shutoffTimeoutInSeconds);
  }

  /**
   * @param {string} profileName The user's full name.
   * @param {string} profileLabel The user's email.
   * @return {string} Localized label.
   * @private
   */
  getAccountRowLabel(profileName, profileLabel) {
    return this.i18n('nearbyShareAccountRowLabel', profileName, profileLabel);
  }

  /** @private */
  getEnabledToggleClassName_() {
    if (this.getPref('nearby_sharing.enabled').value) {
      return 'enabled-toggle-on';
    }
    return 'enabled-toggle-off';
  }

  /** @private */
  onOnboardingCancelled_() {
    // Return to main settings page multidevice section
    Router.getInstance().navigateTo(routes.MULTIDEVICE);
  }

  /** @private */
  onFastInitiationNotificationToggledByUser_() {
    this.set(
        'settings.fastInitiationNotificationState',
        this.isFastInitiationNotificationEnabled_() ?
            nearbyShare.mojom.FastInitiationNotificationState.kDisabledByUser :
            nearbyShare.mojom.FastInitiationNotificationState.kEnabled);
  }

  /**
   * @return {boolean}
   * @private
   */
  isFastInitiationNotificationEnabled_() {
    return this.get('settings.fastInitiationNotificationState') ===
        nearbyShare.mojom.FastInitiationNotificationState.kEnabled;
  }

  /**
   * @param {boolean} isNearbySharingEnabled
   * @param {boolean} isOnboardingComplete
   * @param {boolean} shouldShowFastInititationNotificationToggle
   * @return {boolean}
   * @private
   */
  shouldShowSubpageContent_(
      isNearbySharingEnabled, isOnboardingComplete,
      shouldShowFastInititationNotificationToggle) {
    if (!isOnboardingComplete) {
      return false;
    }
    return isNearbySharingEnabled ||
        shouldShowFastInititationNotificationToggle;
  }

  /** @private */
  showOnboarding_() {
    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot.querySelector('#receiveDialog').showOnboarding();
  }

  /**
   * @param {boolean} is_hardware_supported
   * @return {boolean}
   * @private
   */
  computeShouldShowFastInititationNotificationToggle_(is_hardware_supported) {
    return is_hardware_supported;
  }
}

customElements.define(
    SettingsNearbyShareSubpageElement.is, SettingsNearbyShareSubpageElement);
