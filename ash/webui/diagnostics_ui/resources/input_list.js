// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared_css.js';
import './input_card.js';
import './keyboard_tester.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {ConnectedDevicesObserverInterface, ConnectedDevicesObserverReceiver, InputDataProviderInterface, KeyboardInfo, TouchDeviceInfo, TouchDeviceType} from './diagnostics_types.js';
import {getInputDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'input-list' is responsible for displaying keyboard, touchpad, and
 * touchscreen cards.
 */
Polymer({
  is: 'input-list',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?DiagnosticsBrowserProxy} */
  browserProxy_: null,

  /** @private {?InputDataProviderInterface} */
  inputDataProvider_: null,

  /** @private {?ConnectedDevicesObserverReceiver} */
  connectedDevicesObserverReceiver_: null,

  /** @private {?KeyboardTesterElement} */
  keyboardTester_: null,

  properties: {
    /** @private {!Array<!KeyboardInfo>} */
    keyboards_: {
      type: Array,
      value: () => [],
    },

    /** @private {!Array<!TouchDeviceInfo>} */
    touchpads_: {
      type: Array,
      value: () => [],
    },

    /** @private {!Array<!TouchDeviceInfo>} */
    touchscreens_: {
      type: Array,
      value: () => [],
    },

    /** @protected */
    showTouchpads_: {
      type: Boolean,
      computed: 'computeShowTouchpads_(touchpads_.length)',
    },

    /** @protected */
    showTouchscreens_: {
      type: Boolean,
      computed: 'computeShowTouchscreens_(touchscreens_.length)',
    },
  },

  computeShowTouchpads_(numTouchpads) {
    return numTouchpads > 0 && loadTimeData.getBoolean('isTouchpadEnabled');
  },

  computeShowTouchscreens_(numTouchscreens) {
    return numTouchscreens > 0 &&
        loadTimeData.getBoolean('isTouchscreenEnabled');
  },

  /** @override */
  created() {
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
    this.inputDataProvider_ = getInputDataProvider();
    this.loadInitialDevices_();
    this.observeConnectedDevices_();
  },

  /** @private */
  loadInitialDevices_() {
    this.inputDataProvider_.getConnectedDevices().then((devices) => {
      this.keyboards_ = devices.keyboards;
      this.touchpads_ = devices.touchDevices.filter(
          (device) => device.type === TouchDeviceType.kPointer);
      this.touchscreens_ = devices.touchDevices.filter(
          (device) => device.type === TouchDeviceType.kDirect);
    });
  },

  /** @private */
  observeConnectedDevices_() {
    this.connectedDevicesObserverReceiver_ =
      new ConnectedDevicesObserverReceiver(
        /** @type {!ConnectedDevicesObserverInterface} */(this));
    this.inputDataProvider_.observeConnectedDevices(
      this.connectedDevicesObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardConnected.
   * @param {!KeyboardInfo} newKeyboard
   */
  onKeyboardConnected(newKeyboard) {
    this.push('keyboards_', newKeyboard);
  },

  /**
   * Removes the device with the given evdev ID from one of the device list
   * properties.
   * @param {(string | !Array.<(string | number)>)} path the property's path
   * @param {number} id
   * @private
   */
  removeDeviceById_(path, id) {
    const index = this.get(path).findIndex((device) => device.id === id);
    if (index !== -1) {
      this.splice(path, index, 1);
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnKeyboardDisconnected.
   * @param {number} id
   */
  onKeyboardDisconnected(id) {
    this.removeDeviceById_('keyboards_', id);
    if (this.keyboards_.length === 0 && this.keyboardTester_) {
      // When no keyboards are connected, the <diagnostics-app> component hides
      // the input page. If that happens while a <cr-dialog> is open, the rest
      // of the app remains unresponsive due to the dialog's native logic
      // blocking interaction with other elements. To prevent this we have to
      // explicitly close the dialog when this happens.
      this.keyboardTester_.close();
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceConnected.
   * @param {!TouchDeviceInfo} newTouchDevice
   */
  onTouchDeviceConnected(newTouchDevice) {
    if (newTouchDevice.type === TouchDeviceType.kPointer) {
      this.push('touchpads_', newTouchDevice);
    } else {
      this.push('touchscreens_', newTouchDevice);
    }
  },

  /**
   * Implements ConnectedDevicesObserver.OnTouchDeviceDisconnected.
   * @param {number} id
   */
  onTouchDeviceDisconnected(id) {
    this.removeDeviceById_('touchpads_', id);
    this.removeDeviceById_('touchscreens_', id);
  },

  /**
   * @param {!CustomEvent} e
   * @private
   */
  handleKeyboardTestButtonClick_(e) {
    if (!this.keyboardTester_) {
      this.keyboardTester_ = /** @type {!KeyboardTesterElement} */ (
          document.createElement('keyboard-tester'));
      this.keyboardTester_.setInputDataProvider(
          assert(this.inputDataProvider_));
      this.root.appendChild(this.keyboardTester_);
    }
    this.keyboardTester_.keyboard = assert(
        this.keyboards_.find((keyboard) => keyboard.id === e.detail.evdevId));
    this.keyboardTester_.show();
  },

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   * @param {{isActive: boolean}} isActive
   * @public
   */
  onNavigationPageChanged({isActive}) {
    if (isActive) {
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('input');
    }
  },
});
