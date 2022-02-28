// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Maximum number of bluetooth devices shown in bluetooth subpage.
 * @type {number}
 */
const MAX_NUMBER_DEVICE_SHOWN = 50;

/**
 * @fileoverview
 * 'settings-bluetooth-subpage' is the settings subpage for managing bluetooth
 *  properties and devices.
 */

import {Polymer, html, flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_components/chromeos/bluetooth/bluetooth_dialog.js';
import {CrScrollableBehavior} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {BluetoothUiSurface, recordBluetoothUiSurfaceMetrics, recordUserInitiatedReconnectionAttemptDuration} from '//resources/cr_components/chromeos/bluetooth/bluetooth_metrics_utils.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from '//resources/js/list_property_update_behavior.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {loadTimeData} from '../../i18n_setup.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {Router, Route} from '../../router.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';
import '../../settings_shared_css.js';
import {recordSettingChange} from '../metrics_recorder.m.js';
import './bluetooth_device_list_item.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-bluetooth-subpage',

  behaviors: [
    I18nBehavior,
    CrScrollableBehavior,
    DeepLinkingBehavior,
    ListPropertyUpdateBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** Reflects the bluetooth-page property. */
    bluetoothToggleState: {
      type: Boolean,
      notify: true,
    },

    /** Reflects the bluetooth-page property. */
    stateChangeInProgress: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * The bluetooth adapter state, cached by bluetooth-page.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: Object,

    /** Informs bluetooth-page whether to show the spinner in the header. */
    showSpinner_: {
      type: Boolean,
      notify: true,
      computed: 'computeShowSpinner_(adapterState.*, dialogShown_)',
    },

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     * @private
     */
    deviceList_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The ordered list of paired or connecting bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    pairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * The ordered list of unpaired bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    unpairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * Whether or not the dialog is shown.
     * @private
     */
    dialogShown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,

    /**
     * Interface for bluetooth calls. Set in bluetooth-page.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. Set in bluetooth-page.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },

    /**
     * Update frequency of the bluetooth list.
     * @type {number}
     */
    listUpdateFrequencyMs: {
      type: Number,
      value: 1000,
    },

    /**
     * The time in milliseconds at which discovery was started attempt (when the
     * page was opened with Bluetooth on, or when Bluetooth turned on while the
     * page was active).
     * @private {?number}
     */
    discoveryStartTimestampMs_: {
      type: Number,
      value: null,
    },

    /**
     * Used by FocusRowBehavior to track the last focused element on a row.
     * @private {?Object}
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,

    /**
     * Contains the settingId of any deep link that wasn't able to be shown,
     * null otherwise.
     * @private {?chromeos.settings.mojom.Setting}
     */
    pendingSettingId_: {
      type: Number,
      value: null,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kBluetoothOnOff,
        chromeos.settings.mojom.Setting.kBluetoothConnectToDevice,
        chromeos.settings.mojom.Setting.kBluetoothDisconnectFromDevice,
        chromeos.settings.mojom.Setting.kBluetoothPairDevice,
        chromeos.settings.mojom.Setting.kBluetoothUnpairDevice,
      ]),
    },
  },

  observers: [
    'deviceListChanged_(deviceList_.*)',
    'listUpdateFrequencyMsChanged_(listUpdateFrequencyMs)',
    'updateDiscoveryAndMaybeRefreshDeviceList_(adapterState.*)',
  ],

  /**
   * Timer ID for bluetooth list update.
   * @type {number|undefined}
   * @private
   */
  updateTimerId_: undefined,

  /**
   * Used to prevent duplicate event listeners being added for the focus event.
   * @type {function(Event)|undefined}
   * @private
   */
  onWindowFocusedListener_: undefined,

  /**
   * Used to prevent duplicate event listeners being added for the blur event.
   * @type {function(Event)|undefined}
   * @private
   */
  onWindowBlurredListener_: undefined,

  /**
   * Used to determine if the window has focus. This is overridden by tests so
   * that focus logic can be better encapsulated in this element.
   * @type {function():boolean}
   * @private
   */
  isWindowFocusedFunction_: function() {
    return document.hasFocus();
  },

  /**
   * The address of the device corresponding to the tooltip if it is currently
   * showing. If undefined, the tooltip is not showing.
   * @type {string|undefined}
   * @private
   */
  currentTooltipDeviceAddress_: undefined,

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    // If lastFocused_ is an internal element of a Focus Row (such as the menu
    // button on a paired device), FocusRowBehavior prevents the Focus Row from
    // being focused. We clear lastFocused_ so that we can focus the row (such
    // as a paired/unpaired device).
    if (settingId ===
            chromeos.settings.mojom.Setting.kBluetoothConnectToDevice ||
        settingId ===
            chromeos.settings.mojom.Setting.kBluetoothDisconnectFromDevice ||
        settingId === chromeos.settings.mojom.Setting.kBluetoothPairDevice ||
        settingId === chromeos.settings.mojom.Setting.kBluetoothUnpairDevice) {
      this.lastFocused_ = null;
    }
    // Should continue with deep link attempt.
    return true;
  },

  /** @override */
  detached() {
    if (this.updateTimerId_ !== undefined) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = undefined;
      this.deviceList_ = [];
    }
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    // Any navigation resets the previous attempt to deep link.
    this.pendingSettingId_ = null;
    this.updateDiscoveryAndMaybeRefreshDeviceList_();

    // Does not apply to this page.
    if (route !== routes.BLUETOOTH_DEVICES) {
      this.removeWindowFocusEventListeners_();
      return;
    }

    this.addWindowFocusEventListeners_();

    recordBluetoothUiSurfaceMetrics(
        BluetoothUiSurface.SETTINGS_DEVICE_LIST_SUBPAGE);
    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in refreshBluetoothList_.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  },

  /** @private */
  computeShowSpinner_() {
    return !this.dialogShown_ && this.adapterState &&
        this.adapterState.discovering;
  },

  /** @private */
  updateDiscoveryAndMaybeRefreshDeviceList_() {
    this.updateDiscovery_();
    this.startOrStopRefreshingDeviceList_();
  },

  /** @private */
  deviceListChanged_() {
    this.updateList(
        'pairedDeviceList_', item => item.address,
        this.getUpdatedDeviceList_(
            this.pairedDeviceList_,
            this.deviceList_.filter(d => d.paired || d.connecting)));
    this.updateList(
        'unpairedDeviceList_', item => item.address,
        this.getUpdatedDeviceList_(
            this.unpairedDeviceList_,
            this.deviceList_.filter(d => !(d.paired || d.connecting))));
    this.updateScrollableContents();
  },

  /**
   * Returns a copy of |oldDeviceList| but:
   *   - Using the corresponding device objects in |newDeviceList|
   *   - Removing devices not in |newDeviceList|
   *   - Adding device not in |oldDeviceList| but in |newDeviceList| to the
   *     end of the list.
   *
   * @param {!Array<!chrome.bluetooth.Device>} oldDeviceList
   * @param {!Array<!chrome.bluetooth.Device>} newDeviceList
   * @return {!Array<!chrome.bluetooth.Device>}
   * @private
   */
  getUpdatedDeviceList_(oldDeviceList, newDeviceList) {
    const newDeviceMap = new Map(newDeviceList.map(d => [d.address, d]));
    const updatedDeviceList = [];

    // Add elements of |oldDeviceList| that are in |newDeviceList| to
    // |updatedDeviceList|.
    for (const oldDevice of oldDeviceList) {
      const newDevice = newDeviceMap.get(oldDevice.address);
      if (newDevice === undefined) {
        continue;
      }
      updatedDeviceList.push(newDevice);
      newDeviceMap.delete(newDevice.address);
    }

    // Add all elements of |newDeviceList| that are not in |oldDeviceList| to
    // |updatedDeviceList|.
    for (const newDevice of newDeviceMap.values()) {
      updatedDeviceList.push(newDevice);
    }

    return updatedDeviceList;
  },

  /** @private */
  updateDiscovery_() {
    if (!this.adapterState || !this.adapterState.powered) {
      return;
    }

    // Don't enable discovery if the window isn't focused to avoid keeping the
    // Bluetooth stack in a busy loop.
    if (Router.getInstance().getCurrentRoute() === routes.BLUETOOTH_DEVICES &&
        this.isWindowFocusedFunction_()) {
      this.startDiscovery_();
    } else {
      this.stopDiscovery_();
    }
  },

  /** @private */
  startDiscovery_() {
    if (!this.adapterState || this.adapterState.discovering) {
      return;
    }

    this.bluetooth.startDiscovery(function() {
      const lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message === 'Starting discovery failed') {
          return;
        }  // May happen if also started elsewhere, ignore.
        console.error('startDiscovery Error: ' + lastError.message);
      }
    });
  },

  /** @private */
  stopDiscovery_() {
    if (!this.adapterState || !this.adapterState.discovering) {
      return;
    }

    this.bluetooth.stopDiscovery(function() {
      const lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message === 'Failed to stop discovery') {
          return;
        }  // May happen if also stopped elsewhere, ignore.
        console.error('stopDiscovery Error: ' + lastError.message);
      }
    });
  },

  /**
   * @param {!CustomEvent<!{
   *     action: string, device:
   *     !chrome.bluetooth.Device
   * }>} e
   * @private
   */
  onDeviceEvent_(e) {
    const action = e.detail.action;
    const device = e.detail.device;
    if (action === 'connect') {
      this.connectDevice_(device);
    } else if (action === 'disconnect') {
      this.disconnectDevice_(device);
    } else if (action === 'remove') {
      this.forgetDevice_(device);
    } else {
      console.error('Unexected action: ' + action);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_(event) {
    if (this.isAdapterAvailable_() && !this.stateChangeInProgress) {
      this.bluetoothToggleState = !this.bluetoothToggleState;
    }
    event.stopPropagation();
  },

  /** @private */
  addWindowFocusEventListeners_() {
    // Prevent duplicate event listener registrations by binding the event
    // listener callbacks a single time and storing their values.
    if (!this.onWindowFocusedListener_) {
      this.onWindowFocusedListener_ =
          this.updateDiscoveryAndMaybeRefreshDeviceList_.bind(this);
    }
    if (!this.onWindowBlurredListener_) {
      this.onWindowBlurredListener_ =
          this.updateDiscoveryAndMaybeRefreshDeviceList_.bind(this);
    }
    window.addEventListener('focus', this.onWindowFocusedListener_);
    window.addEventListener('blur', this.onWindowBlurredListener_);
  },

  /** @private */
  removeWindowFocusEventListeners_() {
    if (this.onWindowFocusedListener_) {
      window.removeEventListener('focus', this.onWindowFocusedListener_);
    }
    if (this.onWindowBlurredListener_) {
      window.removeEventListener('blur', this.onWindowBlurredListener_);
    }
  },

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_(enabled, onstr, offstr) {
    // If these strings are changed to convey more information other than "On"
    // and "Off" in the future, revisit the a11y implementation to ensure no
    // meaningful information is skipped.
    return enabled ? onstr : offstr;
  },

  /**
   * @return {boolean}
   * @private
   */
  isAdapterAvailable_() {
    return !!this.adapterState && this.adapterState.available;
  },

  /**
   * @param {boolean} bluetoothToggleState
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showDevices_(bluetoothToggleState, deviceList) {
    return bluetoothToggleState && deviceList.length > 0;
  },

  /**
   * @param {boolean} bluetoothToggleState
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showNoDevices_(bluetoothToggleState, deviceList) {
    return bluetoothToggleState && deviceList.length === 0;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  connectDevice_(device) {
    if (device.connecting || device.connected) {
      return;
    }

    // If the device is not paired, show the pairing dialog before connecting.
    // TODO(crbug.com/966170): Need to check if the device is pairable as well.
    const isPaired = device.paired;
    if (!isPaired) {
      this.pairingDevice_ = device;
      this.openDialog_();
    }

    if (isPaired !== undefined && device.transport !== undefined) {
      this.recordDeviceSelectionDuration_(isPaired, device.transport);
    }

    const connectionStartTimestampMs = Date.now();
    const address = device.address;
    this.bluetoothPrivate.connect(address, result => {
      if (isPaired) {
        const connectResult = chrome.runtime.lastError ? undefined : result;
        chrome.bluetoothPrivate.recordReconnection(connectResult);
        recordUserInitiatedReconnectionAttemptDuration(
            Date.now() - connectionStartTimestampMs, device.transport,
            connectResult);
      }

      // If |pairingDevice_| has changed, ignore the connect result.
      if (this.pairingDevice_ && address !== this.pairingDevice_.address) {
        return;
      }

      // Let the dialog handle any errors, otherwise close the dialog.
      const dialog = this.$.deviceDialog;
      if (dialog.endConnectionAttempt(
              device, !isPaired /* wasPairing */, chrome.runtime.lastError,
              result)) {
        this.openDialog_();
      } else if (
          result !== chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS) {
        this.$.deviceDialog.close();
      }
    });
    recordSettingChange();
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  disconnectDevice_(device) {
    this.bluetoothPrivate.disconnectAll(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error disconnecting: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
    recordSettingChange();
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  forgetDevice_(device) {
    this.bluetoothPrivate.forgetDevice(device.address, () => {
      if (chrome.runtime.lastError) {
        console.error(
            'Error forgetting bluetooth device: ' +
            chrome.runtime.lastError.message);
      }
    });
    recordSettingChange();
  },

  /** @private */
  openDialog_() {
    if (this.dialogShown_) {
      return;
    }
    // Call flush so that the dialog gets sized correctly before it is opened.
    flush();
    this.$.deviceDialog.open();
    recordBluetoothUiSurfaceMetrics(BluetoothUiSurface.SETTINGS_PAIRING_DIALOG);
    this.dialogShown_ = true;
  },

  /** @private */
  onDialogClose_() {
    this.dialogShown_ = false;
    this.pairingDevice_ = undefined;
    // The list is dynamic so focus the first item.
    const device = this.$$('#unpairedContainer bluetooth-device-list-item');
    if (device) {
      device.focus();
    }
  },

  /**
   * Return the sorted devices based on the connected statuses, connected
   * devices are first followed by non-connected devices.
   *
   * @param {!Array<!chrome.bluetooth.Device>} devices
   * @return {!Array<!chrome.bluetooth.Device>}
   * @private
   */
  sortDevices_(devices) {
    return devices.sort((a, b) => a.connected ? -1 : (b.connected ? 1 : 0));
  },

  /**
   * Requests bluetooth device list from Chrome. Update deviceList_ once the
   * results are returned from chrome.
   * @private
   */
  refreshBluetoothList_() {
    const filter = {
      filterType: chrome.bluetooth.FilterType.KNOWN,
      limit: MAX_NUMBER_DEVICE_SHOWN
    };
    this.bluetooth.getDevices(filter, devices => {
      this.deviceList_ = this.sortDevices_(devices);

      // Check if we have yet to focus a deep-linked element.
      if (!this.pendingSettingId_) {
        return;
      }

      this.beforeDeepLinkAttempt(this.pendingSettingId_);
      this.showDeepLink(this.pendingSettingId_).then(result => {
        if (result.deepLinkShown) {
          this.pendingSettingId_ = null;
        }
      });
    });
  },

  /** @private */
  startOrStopRefreshingDeviceList_() {
    if (this.adapterState && this.adapterState.powered) {
      if (this.updateTimerId_ !== undefined) {
        return;
      }

      this.refreshBluetoothList_();
      this.updateTimerId_ = window.setInterval(
          this.refreshBluetoothList_.bind(this), this.listUpdateFrequencyMs);
      this.discoveryStartTimestampMs_ = Date.now();
      return;
    }
    window.clearInterval(this.updateTimerId_);
    this.updateTimerId_ = undefined;
    this.discoveryStartTimestampMs_ = null;
    this.deviceList_ = [];
  },

  /**
   * Restarts the timer when the frequency changes, which happens
   * during tests.
   */
  listUpdateFrequencyMsChanged_() {
    if (this.updateTimerId_ === undefined) {
      return;
    }

    window.clearInterval(this.updateTimerId_);
    this.updateTimerId_ = undefined;

    this.startOrStopRefreshingDeviceList_();
  },

  /**
   * Record metrics for how long it took between when discovery started on the
   * Settings page, and the user selected the device they wanted to connect to.
   * @param {!boolean} wasPaired If the selected device was already
   *     paired.
   * @param {!chrome.bluetooth.Transport} transport The transport type
   *     of the device.
   * @private
   */
  recordDeviceSelectionDuration_(wasPaired, transport) {
    if (!this.discoveryStartTimestampMs_) {
      // It's not necessarily an error that |discoveryStartTimestampMs_| isn't
      // present; it's intentionally cleared after the first device selection
      // (see further on in this method). Recording subsequent device selections
      // after the first would provide inflated durations that don't truly
      // reflect how long it took for the user to find the device they're
      // looking for.
      return;
    }

    chrome.bluetoothPrivate.recordDeviceSelection(
        Date.now() - this.discoveryStartTimestampMs_, wasPaired, transport);

    this.discoveryStartTimestampMs_ = null;
  },

  /**
   * Updates the visibility of the enterprise policy UI tooltip. This is
   * triggered by the blocked-tooltip-state-change event. This event can be
   * fired in two cases:
   * 1) We want to show the tooltip for a given device's icon. Here, show will
   *    be true and the element will be defined.
   * 2) We want to make sure there is no tooltip showing for a given device's
   *    icon. Here, show will be false and the element undefined.
   * In both cases, address will be the item's device address.
   * We need to use a common tooltip since a tooltip within the item gets cut
   * off from the iron-list.
   * @param {!{detail: {address: string, show: boolean, element: ?HTMLElement}}}
   *     e
   * @private
   */
  onBlockedTooltipStateChange_: function(e) {
    const target = e.detail.element;
    const hide = () => {
      /** @type {{hide: Function}} */ (this.$.tooltip).hide();
      this.$.tooltip.removeEventListener('mouseenter', hide);
      this.currentTooltipDeviceAddress_ = undefined;
      if (target) {
        target.removeEventListener('mouseleave', hide);
        target.removeEventListener('blur', hide);
        target.removeEventListener('tap', hide);
      }
    };

    if (!e.detail.show) {
      if (this.currentTooltipDeviceAddress_ &&
          e.detail.address === this.currentTooltipDeviceAddress_) {
        hide();
      }
      return;
    }

    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets. Since paper-tooltip does not expose a public property
    // or method to update the target, the private property |_target| is
    // updated directly.
    this.$.tooltip._target = target;
    /** @type {{updatePosition: Function}} */ (this.$.tooltip).updatePosition();
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
    this.currentTooltipDeviceAddress_ = e.detail.address;
  },
});
