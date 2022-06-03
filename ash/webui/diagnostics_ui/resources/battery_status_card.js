// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BatteryChargeStatus, BatteryChargeStatusObserverInterface, BatteryChargeStatusObserverReceiver, BatteryHealth, BatteryHealthObserverInterface, BatteryHealthObserverReceiver, BatteryInfo, BatteryState, ExternalPowerSource, RoutineType, SystemDataProviderInterface} from './diagnostics_types.js';
import {getDiagnosticsIcon} from './diagnostics_utils.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';
import {TestSuiteStatus} from './routine_list_executor.js';

const BATTERY_ICON_PREFIX = 'battery-';

/**
 * @fileoverview
 * 'battery-status-card' shows information about battery status.
 */
Polymer({
  is: 'battery-status-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * Receiver responsible for observing battery charge status.
   * @private {?BatteryChargeStatusObserverReceiver}
   */
  batteryChargeStatusObserverReceiver_: null,

  /**
   * Receiver responsible for observing battery health.
   * @private {
   *  ?BatteryHealthObserverReceiver}
   */
  batteryHealthObserverReceiver_: null,

  properties: {
    /** @private {!BatteryChargeStatus} */
    batteryChargeStatus_: {
      type: Object,
    },

    /** @private {!BatteryHealth} */
    batteryHealth_: {
      type: Object,
    },

    /** @private {!BatteryInfo} */
    batteryInfo_: {
      type: Object,
    },

    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      computed:
          'getCurrentPowerRoutines_(batteryChargeStatus_.powerAdapterStatus)',
    },

    /** @protected {string} */
    powerTimeString_: {
      type: String,
      computed: 'getPowerTimeString_(batteryChargeStatus_.powerTime)',
    },

    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
      notify: true,
    },

    /** @type {string} */
    batteryIcon: {
      type: String,
      computed: 'getBatteryIcon_(batteryChargeStatus_.powerAdapterStatus,' +
          'batteryChargeStatus_.chargeNowMilliampHours,' +
          'batteryHealth_.chargeFullNowMilliampHours)',
    },

    /** @type {string} */
    iconClass: {
      type: String,
      computed: 'updateIconClassList_(batteryChargeStatus_.powerAdapterStatus)',
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchBatteryInfo_();
    this.observeBatteryChargeStatus_();
    this.observeBatteryHealth_();
  },

  /** @override */
  detached() {
    this.batteryChargeStatusObserverReceiver_.$.close();
    this.batteryHealthObserverReceiver_.$.close();
  },

  /** @private */
  fetchBatteryInfo_() {
    this.systemDataProvider_.getBatteryInfo().then((result) => {
      this.onBatteryInfoReceived_(result.batteryInfo);
    });
  },

  /**
   * @param {!BatteryInfo} batteryInfo
   * @private
   */
  onBatteryInfoReceived_(batteryInfo) {
    this.batteryInfo_ = batteryInfo;
  },

  /** @private */
  observeBatteryChargeStatus_() {
    this.batteryChargeStatusObserverReceiver_ =
        new BatteryChargeStatusObserverReceiver(
            /**
             * @type {!BatteryChargeStatusObserverInterface}
             */
            (this));

    this.systemDataProvider_.observeBatteryChargeStatus(
        this.batteryChargeStatusObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements BatteryChargeStatusObserver.onBatteryChargeStatusUpdated()
   * @param {!BatteryChargeStatus} batteryChargeStatus
   */
  onBatteryChargeStatusUpdated(batteryChargeStatus) {
    this.batteryChargeStatus_ = batteryChargeStatus;
  },

  /** @private */
  observeBatteryHealth_() {
    this.batteryHealthObserverReceiver_ = new BatteryHealthObserverReceiver(
        /**
         * @type {!BatteryHealthObserverInterface}
         */
        (this));

    this.systemDataProvider_.observeBatteryHealth(
        this.batteryHealthObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Get an array of currently relevant routines based on power adaptor status
   * @param {!ExternalPowerSource} powerAdapterStatus
   * @return {!Array<!RoutineType>}
   * @private
   */
  getCurrentPowerRoutines_(powerAdapterStatus) {
    return powerAdapterStatus === ExternalPowerSource.kDisconnected ?
        [RoutineType.kBatteryDischarge] :
        [RoutineType.kBatteryCharge];
  },

  /**
   * Get power time string from battery status.
   * @return {string}
   * @protected
   */
  getPowerTimeString_() {
    const fullyCharged =
        this.batteryChargeStatus_.batteryState === BatteryState.kFull;
    if (fullyCharged) {
      return loadTimeData.getString('batteryFullText');
    }

    const powerTimeStr = this.batteryChargeStatus_.powerTime;
    if (!powerTimeStr || powerTimeStr.data.length === 0) {
      return loadTimeData.getString('batteryCalculatingText');
    }

    const timeValue = mojoString16ToString(powerTimeStr);
    const charging = this.batteryChargeStatus_.powerAdapterStatus ===
        ExternalPowerSource.kAc;

    return charging ?
        loadTimeData.getStringF('batteryChargingStatusText', timeValue) :
        loadTimeData.getStringF('batteryDischargingStatusText', timeValue);
  },

  /**
   * Implements BatteryHealthObserver.onBatteryHealthUpdated()
   * @param {!BatteryHealth} batteryHealth
   */
  onBatteryHealthUpdated(batteryHealth) {
    this.batteryHealth_ = batteryHealth;
  },

  /** @protected */
  getDesignedFullCharge_() {
    return loadTimeData.getStringF(
        'batteryChipText', this.batteryHealth_.chargeFullDesignMilliampHours);
  },

  /** @protected */
  getBatteryHealth_() {
    const MAX_PERCENTAGE = 100;
    const batteryWearPercentage =
        Math.min(this.batteryHealth_.batteryWearPercentage, MAX_PERCENTAGE);
    return loadTimeData.getStringF('batteryHealthText', batteryWearPercentage);
  },

  /** @protected */
  getCurrentNow_() {
    return loadTimeData.getStringF(
        'currentNowText', this.batteryChargeStatus_.currentNowMilliamps);
  },

  /** @protected */
  getRunTestsButtonText_() {
    return loadTimeData.getString(
        this.batteryChargeStatus_.powerAdapterStatus ===
                ExternalPowerSource.kDisconnected ?
            'runBatteryDischargeTestText' :
            'runBatteryChargeTestText');
  },

  /** @protected */
  getRunTestsAdditionalMessage() {
    const batteryInfoMissing =
        !this.batteryChargeStatus_ || !this.batteryHealth_;
    const notCharging = this.batteryChargeStatus_.powerAdapterStatus ===
        ExternalPowerSource.kDisconnected;
    if (notCharging || batteryInfoMissing) {
      return '';
    }

    const disableRunButtonThreshold = 95;
    const percentage = Math.round(
        100 * this.batteryChargeStatus_.chargeNowMilliampHours /
        this.batteryHealth_.chargeFullNowMilliampHours);

    return percentage >= disableRunButtonThreshold ?
        loadTimeData.getString('batteryChargeTestFullMessage') :
        '';
  },

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Power routines will always last <= 1 minute.
    return 1;
  },

  /**
   * Use the current battery percentage to determine which icon to show the
   * user. Each icon covers a range of 6 or 7 percentage values.
   * @private
   * @return {string}
   */
  getBatteryIconForChargePercentage_() {
    if (!this.batteryChargeStatus_ || !this.batteryHealth_) {
      return this.batteryIcon;
    }

    const percentage = Math.round(
        100 * this.batteryChargeStatus_.chargeNowMilliampHours /
        this.batteryHealth_.chargeFullNowMilliampHours);
    assert(percentage > 0 && percentage <= 100);

    const iconSizes = [
      [1, 7],
      [8, 14],
      [15, 21],
      [22, 28],
      [29, 35],
      [36, 42],
      [43, 49],
      [50, 56],
      [57, 63],
      [64, 70],
      [71, 77],
      [78, 85],
      [86, 92],
      [93, 100],
    ];

    for (const [rangeStart, rangeEnd] of iconSizes) {
      if (percentage >= rangeStart && percentage <= rangeEnd) {
        return getDiagnosticsIcon(
            `${BATTERY_ICON_PREFIX}${rangeStart}-${rangeEnd}`);
      }
    }

    assertNotReached();
  },

  /**
   * @protected
   * @return {string}
   */
  getBatteryIcon_() {
    const charging = this.batteryChargeStatus_ &&
        this.batteryChargeStatus_.powerAdapterStatus ===
            ExternalPowerSource.kAc;

    if (charging) {
      return getDiagnosticsIcon(`${BATTERY_ICON_PREFIX}charging`);
    }

    return this.getBatteryIconForChargePercentage_();
  },

  /**
   * Use the power adapter status to determine if we need to overwrite the value
   * for --iron-icon-stroke-color since the charging icon needs to remove it in
   * order to display properly.
   * @protected
   * @return {string}
   */
  updateIconClassList_() {
    return (this.batteryChargeStatus_ &&
            this.batteryChargeStatus_.powerAdapterStatus ===
                ExternalPowerSource.kAc) ?
        'remove-stroke' :
        '';
  }
});
