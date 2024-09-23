// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './battery_status_card.html.js';
import {getDiagnosticsIcon} from './diagnostics_utils.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
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
  static get is(): string {
    return 'battery-status-card';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      batteryChargeStatus: {
        type: Object,
      },

      batteryHealth: {
        type: Object,
      },

      batteryInfo: {
        type: Object,
      },

      routines: {
        type: Array,
        computed:
            'getCurrentPowerRoutines(batteryChargeStatus.powerAdapterStatus)',
      },

      powerTimeString: {
        type: String,
        computed: 'getPowerTimeString(batteryChargeStatus.powerTime)',
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      batteryIcon: {
        type: String,
        computed: 'getBatteryIcon(batteryChargeStatus.powerAdapterStatus,' +
            'batteryChargeStatus.chargeNowMilliampHours,' +
            'batteryHealth.chargeFullNowMilliampHours)',
      },

      iconClass: {
        type: String,
        computed: 'updateIconClassList(batteryChargeStatus.powerAdapterStatus)',
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
  private batteryChargeStatus: BatteryChargeStatus;
  private batteryHealth: BatteryHealth;
  private batteryInfo: BatteryInfo;
  private routines: RoutineType[];
  private powerTimeString: string;
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();
  private batteryChargeStatusObserverReceiver:
      BatteryChargeStatusObserverReceiver|null;
  private batteryHealthObserverReceiver: BatteryHealthObserverReceiver|null;

  constructor() {
    super();
    this.fetchBatteryInfo();
    this.observeBatteryChargeStatus();
    this.observeBatteryHealth();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.batteryChargeStatusObserverReceiver) {
      this.batteryChargeStatusObserverReceiver.$.close();
    }
    if (this.batteryHealthObserverReceiver) {
      this.batteryHealthObserverReceiver.$.close();
    }
  }

  getBatteryChargeStatusForTesting(): BatteryChargeStatus {
    return this.batteryChargeStatus;
  }

  private fetchBatteryInfo(): void {
    this.systemDataProvider.getBatteryInfo().then(
        (result: {batteryInfo: BatteryInfo}) => {
          this.onBatteryInfoReceived(result.batteryInfo);
        });
  }

  private onBatteryInfoReceived(batteryInfo: BatteryInfo): void {
    this.batteryInfo = batteryInfo;
  }

  private observeBatteryChargeStatus(): void {
    this.batteryChargeStatusObserverReceiver =
        new BatteryChargeStatusObserverReceiver(this);

    this.systemDataProvider.observeBatteryChargeStatus(
        this.batteryChargeStatusObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Implements BatteryChargeStatusObserver.onBatteryChargeStatusUpdated()
   */
  onBatteryChargeStatusUpdated(batteryChargeStatus: BatteryChargeStatus): void {
    this.batteryChargeStatus = batteryChargeStatus;
  }

  private observeBatteryHealth(): void {
    this.batteryHealthObserverReceiver =
        new BatteryHealthObserverReceiver(this);

    this.systemDataProvider.observeBatteryHealth(
        this.batteryHealthObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /**
   * Get an array of currently relevant routines based on power adaptor status
   */
  private getCurrentPowerRoutines(powerAdapterStatus: ExternalPowerSource):
      RoutineType[] {
    return powerAdapterStatus === ExternalPowerSource.kDisconnected ?
        [RoutineType.kBatteryDischarge] :
        [RoutineType.kBatteryCharge];
  }

  /**
   * Get power time string from battery status.
   */
  protected getPowerTimeString(): string {
    const fullyCharged =
        this.batteryChargeStatus.batteryState === BatteryState.kFull;
    if (fullyCharged) {
      return loadTimeData.getString('batteryFullText');
    }

    const powerTimeStr = this.batteryChargeStatus.powerTime;
    if (!powerTimeStr || powerTimeStr.data.length === 0) {
      return loadTimeData.getString('batteryCalculatingText');
    }

    const timeValue = mojoString16ToString(powerTimeStr);
    const charging =
        this.batteryChargeStatus.powerAdapterStatus === ExternalPowerSource.kAc;

    return charging ?
        loadTimeData.getStringF('batteryChargingStatusText', timeValue) :
        loadTimeData.getStringF('batteryDischargingStatusText', timeValue);
  }

  /**
   * Implements BatteryHealthObserver.onBatteryHealthUpdated()
   */
  onBatteryHealthUpdated(batteryHealth: BatteryHealth): void {
    this.batteryHealth = batteryHealth;
  }

  protected getDesignedFullCharge(): string {
    return loadTimeData.getStringF(
        'batteryChipText', this.batteryHealth.chargeFullDesignMilliampHours);
  }

  protected getBatteryHealth(): string {
    const MAX_PERCENTAGE = 100;
    const batteryWearPercentage =
        Math.min(this.batteryHealth.batteryWearPercentage, MAX_PERCENTAGE);
    return loadTimeData.getStringF('batteryHealthText', batteryWearPercentage);
  }

  protected getCurrentNow(): string {
    return loadTimeData.getStringF(
        'currentNowText', this.batteryChargeStatus.currentNowMilliamps);
  }

  protected getRunTestsButtonText(): string {
    return loadTimeData.getString(
        this.batteryChargeStatus.powerAdapterStatus ===
                ExternalPowerSource.kDisconnected ?
            'runBatteryDischargeTestText' :
            'runBatteryChargeTestText');
  }

  protected getRunTestsAdditionalMessage(): string {
    const batteryInfoMissing = !this.batteryChargeStatus || !this.batteryHealth;
    const notCharging = this.batteryChargeStatus.powerAdapterStatus ===
        ExternalPowerSource.kDisconnected;
    if (notCharging || batteryInfoMissing) {
      return '';
    }

    const disableRunButtonThreshold = 95;
    const percentage = calculatePowerPercentage(
        this.batteryChargeStatus.chargeNowMilliampHours,
        this.batteryHealth.chargeFullNowMilliampHours);

    return percentage >= disableRunButtonThreshold ?
        loadTimeData.getString('batteryChargeTestFullMessage') :
        '';
  }

  protected getEstimateRuntimeInMinutes(): number {
    // Power routines will always last <= 1 minute.
    return 1;
  }

  /**
   * Use the current battery percentage to determine which icon to show the
   * user. Each icon covers a range of 6 or 7 percentage values.
   */
  private getBatteryIconForChargePercentage(): string {
    if (!this.batteryChargeStatus || !this.batteryHealth) {
      return this.batteryIcon;
    }

    const percentage = calculatePowerPercentage(
        this.batteryChargeStatus.chargeNowMilliampHours,
        this.batteryHealth.chargeFullNowMilliampHours);

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

  protected getBatteryIcon(): string {
    const charging = this.batteryChargeStatus &&
        this.batteryChargeStatus.powerAdapterStatus === ExternalPowerSource.kAc;

    if (charging) {
      return getDiagnosticsIcon(`${BATTERY_ICON_PREFIX}charging`);
    }

    return this.getBatteryIconForChargePercentage();
  }

  /**
   * Use the power adapter status to determine if we need to overwrite the value
   * for --iron-icon-stroke-color since the charging icon needs to remove it in
   * order to display properly.
   */
  protected updateIconClassList(): string {
    return (this.batteryChargeStatus &&
            this.batteryChargeStatus.powerAdapterStatus ===
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
