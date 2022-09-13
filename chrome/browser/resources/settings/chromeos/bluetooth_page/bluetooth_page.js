// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'Settings page for managing bluetooth properties and devices. This page
 * just provodes a summary and link to the subpage.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '../os_icons.js';
import '../../prefs/prefs.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import './bluetooth_subpage.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

export const bluetoothApis = window['bluetoothApis'] || {
  /**
   * Set this to provide a fake implementation for testing.
   * @type {Bluetooth}
   */
  bluetoothApiForTest: null,

  /**
   * Set this to provide a fake implementation for testing.
   * @type {BluetoothPrivate}
   */
  bluetoothPrivateApiForTest: null,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsBluetoothPageElementBase = mixinBehaviors(
    [I18nBehavior, DeepLinkingBehavior, PrefsBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class SettingsBluetoothPageElement extends SettingsBluetoothPageElementBase {
  static get is() {
    return 'settings-bluetooth-page';
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

      /**
       * Reflects the current state of the toggle buttons (in this page and the
       * subpage). This will be set when the adapter state change or when the
       * user changes the toggle.
       * @private
       */
      bluetoothToggleState_: {
        type: Boolean,
        observer: 'bluetoothToggleStateChanged_',
      },

      /**
       * Set to true while an adapter state change is requested and the callback
       * hasn't fired yet. One of the factor that determines whether to disable
       * the toggle button.
       * @private
       */
      stateChangeInProgress_: {
        type: Boolean,
        value: false,
      },

      /**
       * The cached bluetooth adapter state.
       * @type {!chrome.bluetooth.AdapterState|undefined}
       * @private
       */
      adapterState_: {
        type: Object,
        notify: true,
      },

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.BLUETOOTH_DEVICES) {
            map.set(
                routes.BLUETOOTH_DEVICES.path,
                '#bluetoothDevices .subpage-arrow');
          }
          return map;
        },
      },

      /**
       * Interface for bluetooth calls. May be overridden by tests.
       * @type {Bluetooth}
       * @private
       */
      bluetooth: {
        type: Object,
        value: chrome.bluetooth,
      },

      /**
       * Interface for bluetoothPrivate calls. May be overridden by tests.
       * @type {BluetoothPrivate}
       * @private
       */
      bluetoothPrivate: {
        type: Object,
        value: chrome.bluetoothPrivate,
      },

      /**
       * Whether the user is a secondary user.
       * @private
       */
      isSecondaryUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSecondaryUser');
        },
        readOnly: true,
      },

      /**
       * Email address for the primary user.
       * @private
       */
      primaryUserEmail_: {
        type: String,
        value() {
          return loadTimeData.getString('primaryUserEmail');
        },
        readOnly: true,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kBluetoothOnOff]),
      },

    };
  }

  static get observers() {
    return ['deviceListChanged_(deviceList_.*)'];
  }

  /** @override */
  ready() {
    super.ready();

    if (bluetoothApis.bluetoothApiForTest) {
      this.bluetooth = bluetoothApis.bluetoothApiForTest;
    }
    if (bluetoothApis.bluetoothPrivateApiForTest) {
      this.bluetoothPrivate = bluetoothApis.bluetoothPrivateApiForTest;
    }
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    /**
     * Listener for chrome.bluetooth.onAdapterStateChanged events.
     * @type {function(!chrome.bluetooth.AdapterState)|undefined}
     * @private
     */
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    this.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    // Request the inital adapter state.
    this.bluetooth.getAdapterState(this.bluetoothAdapterStateChangedListener_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.bluetoothAdapterStateChangedListener_) {
      this.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    // Does not apply to this page.
    if (newRoute !== routes.BLUETOOTH) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {boolean} bluetoothToggleState
   * @return {string}
   * @private
   */
  getIcon_(bluetoothToggleState) {
    // Don't use |this.bluetoothToggleState_| here, since it has not been
    // updated yet to the latest value.
    if (!bluetoothToggleState) {
      return 'os-settings:bluetooth-disabled';
    }
    return 'cr:bluetooth';
  }

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_(enabled, onstr, offstr) {
    return enabled ? onstr : offstr;
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleEnabled_() {
    return this.adapterState_ !== undefined && this.adapterState_.available &&
        !this.stateChangeInProgress_;
  }

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_(state) {
    this.adapterState_ = state;
    if (this.isToggleEnabled_()) {
      this.bluetoothToggleState_ = state.powered;
    }
  }

  /**
   * Listens for toggle change events (rather than state changes) to handle
   * just user-triggered changes to the bluetoothToggleState_.
   * @private
   */
  onBluetoothToggledByUser_() {
    // Record that the user manually enabled/disabled Bluetooth.
    recordSettingChange(
        Setting.kBluetoothOnOff, {boolValue: this.bluetoothToggleState_});
  }

  /** @private */
  onTap_() {
    if (!this.isToggleEnabled_()) {
      return;
    }
    if (!this.bluetoothToggleState_) {
      this.bluetoothToggleState_ = true;
    } else {
      this.openSubpage_();
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSubpageArrowTap_(e) {
    this.openSubpage_();
    e.stopPropagation();
  }

  /** @private */
  bluetoothToggleStateChanged_() {
    if (!this.adapterState_ || !this.isToggleEnabled_() ||
        this.bluetoothToggleState_ === this.adapterState_.powered) {
      return;
    }
    this.stateChangeInProgress_ = true;
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothToggleState_}, () => {
          // Restore the in-progress mark when the callback is called regardless
          // of error or success.
          this.stateChangeInProgress_ = false;

          const error = chrome.runtime.lastError;
          if (error && error !== 'Error setting adapter properties: powered') {
            console.error('Error enabling bluetooth: ' + error.message);
            return;
          }
          this.setPrefValue(
              'ash.user.bluetooth.adapter_enabled',
              this.bluetoothToggleState_);
        });
  }

  /** @private */
  openSubpage_() {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
  }
}

customElements.define(
    SettingsBluetoothPageElement.is, SettingsBluetoothPageElement);
