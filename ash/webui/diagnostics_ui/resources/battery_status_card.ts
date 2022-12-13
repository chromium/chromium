// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './battery_status_card.html.js';
import {getDiagnosticsIcon} from './diagnostics_utils.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {BatteryChargeStatus, BatteryChargeStatusObserverReceiver, BatteryHealth, BatteryHealthObserverReceiver, BatteryInfo, BatteryState, ExternalPowerSource, SystemDataProviderInterface} from './system_data_provider.mojom-webui.js';
import {RoutineType} from './system_routine_controller.mojom-webui.js';

const BATTERY_ICON_PREFIX = 'battery-';

/**
 * Calculates the battery percentage in the range percentage [0,100].
 * @param chargeNow Current battery charge in milliamp hours.
 * @param chargeFull Full battery charge in milliamp hours.
 */
function calculatePowerPercentage(
    chargeNow: number, chargeFull: number): number {
  // Handle values in battery_info which could cause a SIGFPE. See b/227485637.
  if (chargeFull == 0 || isNaN(chargeNow) || isNaN(chargeFull)) {
    return 0;
  }

  const percent = Math.round(100 * chargeNow / chargeFull);
  return Math.min(Math.max(percent, 0), 100);
}

/**
 * @fileoverview
 * 'battery-status-card' shows information about battery status.
 */

const BatteryStatusCardElementBase = I18nMixin(PolymerElement);

export class BatteryStatusCardElement extends BatteryStatusCardElementBase {
  static get is() {
    return 'battery-status-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      batteryChargeStatus_: {
        type: Object,
      },

      batteryHealth_: {
        type: Object,
      },

      batteryInfo_: {
        type: Object,
      },

      routines_: {
        type: Array,
        computed:
            'getCurrentPowerRoutines_(batteryChargeStatus_.powerAdapterStatus)',
      },

      powerTimeString_: {
        type: String,
        computed: 'getPowerTimeString_(batteryChargeStatus_.powerTime)',
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      batteryIcon: {
        type: String,
        computed: 'getBatteryIcon_(batteryChargeStatus_.powerAdapterStatus,' +
            'batteryChargeStatus_.chargeNowMilliampHours,' +
            'batteryHealth_.chargeFullNowMilliampHours)',
      },

      iconClass: {
        type: String,
        computed:
            'updateIconClassList_(batteryChargeStatus_.powerAdapterStatus)',
      },

      isActive: {
        type: Boolean,
      },

    };
  }


  testSuiteStatus: TestSuiteStatus;
  batteryIcon: string;
  iconClass: string;
  isActive: boolean;
  private batteryChargeStatus_: BatteryChargeStatus;
  private batteryHealth_: BatteryHealth;
  private batteryInfo_: BatteryInfo;
  private routines_: RoutineType[];
  private powerTimeString_: string;
  private systemDataProvider_: SystemDataProviderInterface =
      getSystemDataProvider();
  private batteryChargeStatusObserverReceiver_:
      BatteryChargeStatusObserverReceiver|null;
  private batteryHealthObserverReceiver_: BatteryHealthObserverReceiver|null;

  constructor() {
    super();
    this.fetchBatteryInfo_();
    this.observeBatteryChargeStatus_();
    this.observeBatteryHealth_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.batteryChargeStatusObserverReceiver_) {
      this.batteryChargeStatusObserverReceiver_.$.close();
    }
    if (this.batteryHealthObserverReceiver_) {
      this.batteryHealthObserverReceiver_.$.close();
    }
  }

  private fetchBatteryInfo_(): void {
    this.systemDataProvider_.getBatteryInfo().then(
        (result: {batteryInfo: BatteryInfo}) => {
          this.onBatteryInfoReceived_(result.batteryInfo);
        });
  }

  private onBatteryInfoReceived_(batteryInfo: BatteryInfo): void {
    this.batteryInfo_ = batteryInfo;
  }

  private observeBatteryChargeStatus_(): void {
    this.batteryChargeStatusObserverReceiver_ =
        new BatteryChargeStatusObserverReceiver(this);

    this.systemDataProvider_.observeBatteryChargeStatus(
        this.batteryChargeStatusObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements BatteryChargeStatusObserver.onBatteryChargeStatusUpdated()
   */
  onBatteryChargeStatusUpdated(batteryChargeStatus: BatteryChargeStatus): void {
    this.batteryChargeStatus_ = batteryChargeStatus;
  }

  private observeBatteryHealth_(): void {
    this.batteryHealthObserverReceiver_ =
        new BatteryHealthObserverReceiver(this);

    this.systemDataProvider_.observeBatteryHealth(
        this.batteryHealthObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * Get an array of currently relevant routines based on power adaptor status
   */
  private getCurrentPowerRoutines_(powerAdapterStatus: ExternalPowerSource):
      RoutineType[] {
    return powerAdapterStatus === ExternalPowerSource.kDisconnected ?
        [RoutineType.kBatteryDischarge] :
        [RoutineType.kBatteryCharge];
  }

  /**
   * Get power time string from battery status.
   */
  protected getPowerTimeString_(): string {
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
  }

  /**
   * Implements BatteryHealthObserver.onBatteryHealthUpdated()
   */
  onBatteryHealthUpdated(batteryHealth: BatteryHealth): void {
    this.batteryHealth_ = batteryHealth;
  }

  protected getDesignedFullCharge_(): string {
    return loadTimeData.getStringF(
        'batteryChipText', this.batteryHealth_.chargeFullDesignMilliampHours);
  }

  protected getBatteryHealth_(): string {
    const MAX_PERCENTAGE = 100;
    const batteryWearPercentage =
        Math.min(this.batteryHealth_.batteryWearPercentage, MAX_PERCENTAGE);
    return loadTimeData.getStringF('batteryHealthText', batteryWearPercentage);
  }

  protected getCurrentNow_(): string {
    return loadTimeData.getStringF(
        'currentNowText', this.batteryChargeStatus_.currentNowMilliamps);
  }

  protected getRunTestsButtonText_(): string {
    return loadTimeData.getString(
        this.batteryChargeStatus_.powerAdapterStatus ===
                ExternalPowerSource.kDisconnected ?
            'runBatteryDischargeTestText' :
            'runBatteryChargeTestText');
  }

  protected getRunTestsAdditionalMessage(): string {
    const batteryInfoMissing =
        !this.batteryChargeStatus_ || !this.batteryHealth_;
    const notCharging = this.batteryChargeStatus_.powerAdapterStatus ===
        ExternalPowerSource.kDisconnected;
    if (notCharging || batteryInfoMissing) {
      return '';
    }

    const disableRunButtonThreshold = 95;
    const percentage = calculatePowerPercentage(
        this.batteryChargeStatus_.chargeNowMilliampHours,
        this.batteryHealth_.chargeFullNowMilliampHours);

    return percentage >= disableRunButtonThreshold ?
        loadTimeData.getString('batteryChargeTestFullMessage') :
        '';
  }

  protected getEstimateRuntimeInMinutes_(): number {
    // Power routines will always last <= 1 minute.
    return 1;
  }

  /**
   * Use the current battery percentage to determine which icon to show the
   * user. Each icon covers a range of 6 or 7 percentage values.
   */
  private getBatteryIconForChargePercentage_(): string {
    if (!this.batteryChargeStatus_ || !this.batteryHealth_) {
      return this.batteryIcon;
    }

    const percentage = calculatePowerPercentage(
        this.batteryChargeStatus_.chargeNowMilliampHours,
        this.batteryHealth_.chargeFullNowMilliampHours);

    // Handle values in battery_info which could cause a SIGFPE. See
    // b/227485637.
    if (percentage === 0) {
      return this.batteryIcon ||
          getDiagnosticsIcon(`${BATTERY_ICON_PREFIX}outline`);
    }

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
  }

  protected getBatteryIcon_(): string {
    const charging = this.batteryChargeStatus_ &&
        this.batteryChargeStatus_.powerAdapterStatus ===
            ExternalPowerSource.kAc;

    if (charging) {
      return getDiagnosticsIcon(`${BATTERY_ICON_PREFIX}charging`);
    }

    return this.getBatteryIconForChargePercentage_();
  }

  /**
   * Use the power adapter status to determine if we need to overwrite the value
   * for --iron-icon-stroke-color since the charging icon needs to remove it in
   * order to display properly.
   */
  protected updateIconClassList_(): string {
    return (this.batteryChargeStatus_ &&
            this.batteryChargeStatus_.powerAdapterStatus ===
                ExternalPowerSource.kAc) ?
        'remove-stroke' :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'battery-status-card': BatteryStatusCardElement;
  }
}

customElements.define(BatteryStatusCardElement.is, BatteryStatusCardElement);
