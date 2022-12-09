// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage providing high level summary of the state of Bluetooth and
 * its connected devices.
 */

import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {getDeviceName} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Router} from '../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

/**
 * Refers to Bluetooth secondary text label, used to distinguish between
 * accessibility string and UI text string.
 * @enum {number}
 */
const LabelType = {
  A11Y: 1,
  DISPLAYED_TEXT: 2,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 */
const SettingsBluetoothSummaryElementBase = mixinBehaviors(
    [I18nBehavior, RouteObserverBehavior, RouteOriginBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothSummaryElement extends
    SettingsBluetoothSummaryElementBase {
  static get is() {
    return 'os-settings-bluetooth-summary';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!BluetoothSystemProperties}
       */
      systemProperties: {
        type: Object,
        observer: 'onSystemPropertiesChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |systemProperties| state changes or when the user presses the
       * toggle.
       * @private
       */
      isBluetoothToggleOn_: {
        type: Boolean,
        observer: 'onBluetoothToggleChanged_',
      },

      /** @private */
      LabelType: {
        type: Object,
        value: LabelType,
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
    };
  }

  constructor() {
    super();

    /** RouteOriginBehaviorInterface override */
    this.route_ = routes.BASIC;
  }

  /** @override */
  ready() {
    super.ready();

    this.addFocusConfig(routes.BLUETOOTH_DEVICES, '.subpage-arrow');
  }

  /** @private */
  onSystemPropertiesChanged_() {
    this.isBluetoothToggleOn_ =
        this.systemProperties.systemState === BluetoothSystemState.kEnabled ||
        this.systemProperties.systemState === BluetoothSystemState.kEnabling;
  }

  /**
   * Observer for isBluetoothToggleOn_ that returns early until the previous
   * value was not undefined to avoid wrongly toggling the Bluetooth state.
   * @param {boolean} newValue
   * @param {boolean} oldValue
   * @private
   */
  onBluetoothToggleChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      return;
    }
    // If the toggle value changed but the toggle is disabled, the change came
    // from CrosBluetoothConfig, not the user. Don't attempt to update the
    // enabled state.
    if (this.isToggleDisabled_()) {
      return;
    }
    getBluetoothConfig().setBluetoothEnabledState(this.isBluetoothToggleOn_);
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    if (!this.systemProperties) {
      return false;
    }
    // TODO(crbug.com/1010321): Add check for modification state when variable
    // is available.
    return this.systemProperties.systemState ===
        BluetoothSystemState.kUnavailable;
  }

  /**
   * @param {LabelType} labelType
   * @return {string}
   * @private
   */
  getSecondaryLabel_(labelType) {
    if (!this.isBluetoothToggleOn_) {
      return this.i18n('bluetoothSummaryPageOff');
    }

    const connectedDevices = this.getConnectedDevices_();

    if (!connectedDevices.length) {
      return this.i18n('bluetoothSummaryPageOn');
    }

    const isA11yLabel = labelType === LabelType.A11Y;
    const firstConnectedDeviceName = getDeviceName(connectedDevices[0]);

    if (connectedDevices.length === 1) {
      return isA11yLabel ? this.i18n(
                               'bluetoothSummaryPageConnectedA11yOneDevice',
                               firstConnectedDeviceName) :
                           firstConnectedDeviceName;
    }

    if (connectedDevices.length === 2) {
      const secondConnectedDeviceName = getDeviceName(connectedDevices[1]);
      return isA11yLabel ?
          this.i18n(
              'bluetoothSummaryPageConnectedA11yTwoDevices',
              firstConnectedDeviceName, secondConnectedDeviceName) :
          this.i18n(
              'bluetoothSummaryPageTwoDevicesDescription',
              firstConnectedDeviceName, secondConnectedDeviceName);
    }

    return isA11yLabel ?
        this.i18n(
            'bluetoothSummaryPageConnectedA11yTwoOrMoreDevices',
            firstConnectedDeviceName, connectedDevices.length - 1) :
        this.i18n(
            'bluetoothSummaryPageTwoOrMoreDevicesDescription',
            firstConnectedDeviceName, connectedDevices.length - 1);
  }

  /**
   * @return {Array<?PairedBluetoothDeviceProperties>}
   * @private
   */
  getConnectedDevices_() {
    const pairedDevices = this.systemProperties.pairedDevices;
    if (!pairedDevices) {
      return [];
    }

    return pairedDevices.filter(
        device => device.deviceProperties.connectionState ===
            DeviceConnectionState.kConnected);
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStatusIconName_() {
    if (!this.isBluetoothToggleOn_) {
      return 'os-settings:bluetooth-disabled';
    }

    if (this.getConnectedDevices_().length) {
      return 'os-settings:bluetooth-connected';
    }
    return 'cr:bluetooth';
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSubpageArrow_() {
    if (this.isToggleDisabled_()) {
      return false;
    }

    return this.isBluetoothToggleOn_;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSubpageArrowClick_(e) {
    this.navigateToBluetoothDevicesSubpage_();
    e.stopPropagation();
  }

  /** @private */
  navigateToBluetoothDevicesSubpage_() {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
  }

  /** @private */
  onWrapperClick_() {
    if (this.isToggleDisabled_()) {
      return;
    }

    if (this.systemProperties.systemState === BluetoothSystemState.kDisabled ||
        this.systemProperties.systemState === BluetoothSystemState.kDisabling) {
      this.isBluetoothToggleOn_ = true;
      return;
    }
    this.navigateToBluetoothDevicesSubpage_();
  }

  /** @private */
  onPairNewDeviceBtnClick_() {
    this.dispatchEvent(new CustomEvent('start-pairing', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  annouceBluetoothStateChange_() {
    getAnnouncerInstance().announce(
        this.isBluetoothToggleOn_ ? this.i18n('bluetoothEnabledA11YLabel') :
                                    this.i18n('bluetoothDisabledA11YLabel'));
  }
}

customElements.define(
    SettingsBluetoothSummaryElement.is, SettingsBluetoothSummaryElement);
