// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'Settings page for managing bluetooth properties and devices. This page
 * just provodes a summary and link to the subpage.
 */

const bluetoothApis = window['bluetoothApis'] || {
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

Polymer({
  is: 'settings-bluetooth-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Reflects the current state of the toggle buttons (in this page and the
     * subpage). This will be set when the adapter state change or when the user
     * changes the toggle.
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
      value: function() {
        const map = new Map();
        if (settings.routes.BLUETOOTH_DEVICES) {
          map.set(
              settings.routes.BLUETOOTH_DEVICES.path,
              '#bluetoothDevices .subpage-arrow');
        }
        return map;
      },
    },

    /**
     * Interface for bluetooth calls. May be overriden by tests.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. May be overriden by tests.
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
      value: function() {
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
      value: function() {
        return loadTimeData.getString('primaryUserEmail');
      },
      readOnly: true,
    },
  },

  observers: ['deviceListChanged_(deviceList_.*)'],

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)|undefined}
   * @private
   */
  bluetoothAdapterStateChangedListener_: undefined,

  /** @override */
  ready: function() {
    if (bluetoothApis.bluetoothApiForTest) {
      this.bluetooth = bluetoothApis.bluetoothApiForTest;
    }
    if (bluetoothApis.bluetoothPrivateApiForTest) {
      this.bluetoothPrivate = bluetoothApis.bluetoothPrivateApiForTest;
    }
  },

  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    this.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    // Request the inital adapter state.
    this.bluetooth.getAdapterState(this.bluetoothAdapterStateChangedListener_);
  },

  /** @override */
  detached: function() {
    if (this.bluetoothAdapterStateChangedListener_) {
      this.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
  },

  /**
   * @param {boolean} bluetoothToggleState
   * @return {string}
   * @private
   */
  getIcon_: function(bluetoothToggleState) {
    // Don't use |this.bluetoothToggleState_| here, since it has not been
    // updated yet to the latest value.
    if (!bluetoothToggleState) {
      return 'os-settings:bluetooth-disabled';
    }
    return 'cr:bluetooth';
  },

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_: function(enabled, onstr, offstr) {
    return enabled ? onstr : offstr;
  },

  /**
   * @return {boolean}
   * @private
   */
  isToggleEnabled_: function() {
    return this.adapterState_ !== undefined && this.adapterState_.available &&
        !this.stateChangeInProgress_;
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.adapterState_ = state;
    if (this.isToggleEnabled_()) {
      this.bluetoothToggleState_ = state.powered;
    }
  },

  /** @private */
  onTap_: function() {
    if (!this.isToggleEnabled_()) {
      return;
    }
    if (!this.bluetoothToggleState_) {
      this.bluetoothToggleState_ = true;
    } else {
      this.openSubpage_();
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onSubpageArrowTap_: function(e) {
    this.openSubpage_();
    e.stopPropagation();
  },

  /** @private */
  bluetoothToggleStateChanged_: function() {
    if (!this.adapterState_ || !this.isToggleEnabled_() ||
        this.bluetoothToggleState_ == this.adapterState_.powered) {
      return;
    }
    this.stateChangeInProgress_ = true;
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothToggleState_}, () => {
          // Restore the in-progress mark when the callback is called regardless
          // of error or success.
          this.stateChangeInProgress_ = false;

          const error = chrome.runtime.lastError;
          if (error && error != 'Error setting adapter properties: powered') {
            console.error('Error enabling bluetooth: ' + error.message);
            return;
          }
          this.setPrefValue(
              'ash.user.bluetooth.adapter_enabled',
              this.bluetoothToggleState_);
        });
  },

  /** @private */
  openSubpage_: function() {
    settings.navigateTo(settings.routes.BLUETOOTH_DEVICES);
  }
});
