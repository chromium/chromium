// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {getTemplate} from './phone_status_model_form.html.js';
import {BatterySaverState, ChargingState, MobileStatus, PhoneStatusModel, SignalStrength} from './types.js';

/**
 * Maps a MobileStatus to its title label in the dropdown.
 * @type {!Map<MobileStatus, String>}
 */
const mobileStatusToStringMap = new Map([
  [MobileStatus.NO_SIM, 'No SIM'],
  [MobileStatus.SIM_BUT_NO_RECEPTION, 'SIM but no reception'],
  [MobileStatus.SIM_WITH_RECEPTION, 'SIM with reception'],
]);

/**
 * Maps a SignalStrength to its title label in the dropdown.
 * @type {!Map<SignalStrength, String>}
 */
const signalStrengthToStringMap = new Map([
  [SignalStrength.ZERO_BARS, 'Zero bars'],
  [SignalStrength.ONE_BAR, 'One bar'],
  [SignalStrength.TWO_BARS, 'Two bars'],
  [SignalStrength.THREE_BARS, 'Three bars'],
  [SignalStrength.FOUR_BARS, 'Four bars'],
]);

/**
 * Maps a ChargingState to its title label in the dropdown.
 * @type {!Map<ChargingState, String>}
 */
const chargingStateToStringMap = new Map([
  [ChargingState.NOT_CHARGING, 'Not charging'],
  [ChargingState.CHARGING_AC, 'Charging AC'],
  [ChargingState.CHARGING_USB, 'Charging USB'],
]);

/**
 * Maps a BatterySaverState to its title label in the dropdown.
 * @type {!Map<BatterySaverState, String>}
 */
const batterySaverStateToStringMap = new Map([
  [BatterySaverState.OFF, 'Off'],
  [BatterySaverState.ON, 'On'],
]);

Polymer({
  is: 'phone-status-model-form',

  _template: getTemplate(),

  properties: {
    /** @private{MobileStatus} */
    mobileStatus_: {
      type: Number,
      value: MobileStatus.NO_SIM,
    },

    /** @private{SignalStrength}*/
    signalStrength_: {
      type: Number,
      value: SignalStrength.ZERO_BARS,
    },

    /** @private */
    mobileProvider_: {
      type: String,
      value: 'Fake Carrier',
    },

    /** @private{ChargingState} */
    chargingState_: {
      type: Number,
      value: ChargingState.NOT_CHARGING,
    },

    /** @private{BatterySaverState} */
    batterySaverState_: {
      type: Number,
      value: BatterySaverState.OFF,
    },

    /** @private */
    batteryPercentage_: {
      type: Number,
      value: 50,
    },

    /** @private */
    mobileStatusList_: {
      type: Array,
      value: () => {
        return [
          MobileStatus.NO_SIM,
          MobileStatus.SIM_BUT_NO_RECEPTION,
          MobileStatus.SIM_WITH_RECEPTION,
        ];
      },
      readonly: true,
    },

    /** @private */
    signalStrengthList_: {
      type: Array,
      value: () => {
        return [
          SignalStrength.ZERO_BARS,
          SignalStrength.ONE_BAR,
          SignalStrength.TWO_BARS,
          SignalStrength.THREE_BARS,
          SignalStrength.FOUR_BARS,
        ];
      },
      readonly: true,
    },

    /** @private */
    chargingStateList_: {
      type: Array,
      value: () => {
        return [
          ChargingState.NOT_CHARGING,
          ChargingState.CHARGING_AC,
          ChargingState.CHARGING_USB,
        ];
      },
      readonly: true,
    },

    /** @private */
    batterySaverStateList_: {
      type: Array,
      value: () => {
        return [
          BatterySaverState.OFF,
          BatterySaverState.ON,
        ];
      },
      readonly: true,
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
    this.setFakePhoneModel_();
  },

  /** @private */
  onBatteryPercentageInputChanged_() {
    const inputValue = this.$$('#batteryPercentageInput').value;
    if (inputValue > 100) {
      this.batteryPercentage_ = 100;
      return;
    }

    if (inputValue < 0) {
      this.batteryPercentage_ = 0;
      return;
    }

    this.batteryPercentage_ = Number(inputValue);
  },

  /** @private */
  setFakePhoneModel_() {
    const phoneStatusModel = {
      mobileStatus: this.mobileStatus_,
      signalStrength: this.signalStrength_,
      mobileProvider: this.mobileProvider_,
      chargingState: this.chargingState_,
      batterySaverState: this.batterySaverState_,
      batteryPercentage: this.batteryPercentage_,
    };
    this.browserProxy_.setFakePhoneStatus(phoneStatusModel);
  },

  /** @private */
  onMobileStatusSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#mobileStatusList'));
    this.mobileStatus_ = this.mobileStatusList_[select.selectedIndex];
  },

  /** @private */
  onSignalStrengthSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#signalStrengthList'));
    this.signalStrength_ = this.signalStrengthList_[select.selectedIndex];
  },

  /** @private */
  onChargingStateSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#chargingStateList'));
    this.chargingState_ = this.chargingStateList_[select.selectedIndex];
  },

  /** @private */
  onBatterySaverStateSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#batterySaverStateList'));
    this.batterySaverState_ = this.batterySaverStateList_[select.selectedIndex];
  },

  /**
   * @param {MobileStatus} mobileStatus
   * @return {String}
   * @private
   */
  getMobileStatusName_(mobileStatus) {
    return mobileStatusToStringMap.get(mobileStatus);
  },

  /**
   * @param {SignalStrength} signalStrength
   * @return {String}
   * @private
   */
  getSignalStrengthName_(signalStrength) {
    return signalStrengthToStringMap.get(signalStrength);
  },

  /**
   * @param {ChargingState} chargingState
   * @return {String}
   * @private
   */
  getChargingStateName_(chargingState) {
    return chargingStateToStringMap.get(chargingState);
  },

  /**
   * @param {BatterySaverState} batterySaverState
   * @return {String}
   * @private
   */
  getBatterySaverStateName_(batterySaverState) {
    return batterySaverStateToStringMap.get(batterySaverState);
  },
});
