// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './text_badge.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getRoutineFailureMessage} from './diagnostics_utils.js';
import {RoutineGroup} from './routine_group.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {getTemplate} from './routine_result_entry.html.js';
import {RoutineResult, RoutineType, StandardRoutineResult} from './system_routine_controller.mojom-webui.js';
import {BadgeType} from './text_badge.js';

/**
 * Resolves a routine name to its corresponding localized string name.
 */
export function getRoutineType(routineType: RoutineType): string {
  switch (routineType) {
    case RoutineType.kBatteryCharge:
      return loadTimeData.getString('batteryChargeRoutineText');
    case RoutineType.kBatteryDischarge:
      return loadTimeData.getString('batteryDischargeRoutineText');
    case RoutineType.kCaptivePortal:
      return loadTimeData.getString('captivePortalRoutineText');
    case RoutineType.kCpuCache:
      return loadTimeData.getString('cpuCacheRoutineText');
    case RoutineType.kCpuStress:
      return loadTimeData.getString('cpuStressRoutineText');
    case RoutineType.kCpuFloatingPoint:
      return loadTimeData.getString('cpuFloatingPointAccuracyRoutineText');
    case RoutineType.kCpuPrime:
      return loadTimeData.getString('cpuPrimeSearchRoutineText');
    case RoutineType.kDnsLatency:
      return loadTimeData.getString('dnsLatencyRoutineText');
    case RoutineType.kDnsResolution:
      return loadTimeData.getString('dnsResolutionRoutineText');
    case RoutineType.kDnsResolverPresent:
      return loadTimeData.getString('dnsResolverPresentRoutineText');
    case RoutineType.kGatewayCanBePinged:
      return loadTimeData.getString('gatewayCanBePingedRoutineText');
    case RoutineType.kHasSecureWiFiConnection:
      return loadTimeData.getString('hasSecureWiFiConnectionRoutineText');
    case RoutineType.kHttpFirewall:
      return loadTimeData.getString('httpFirewallRoutineText');
    case RoutineType.kHttpsFirewall:
      return loadTimeData.getString('httpsFirewallRoutineText');
    case RoutineType.kHttpsLatency:
      return loadTimeData.getString('httpsLatencyRoutineText');
    case RoutineType.kLanConnectivity:
      return loadTimeData.getString('lanConnectivityRoutineText');
    case RoutineType.kMemory:
      return loadTimeData.getString('memoryRoutineText');
    case RoutineType.kSignalStrength:
      return loadTimeData.getString('signalStrengthRoutineText');
    case RoutineType.kArcHttp:
      return loadTimeData.getString('arcHttpRoutineText');
    case RoutineType.kArcPing:
      return loadTimeData.getString('arcPingRoutineText');
    case RoutineType.kArcDnsResolution:
      return loadTimeData.getString('arcDnsResolutionRoutineText');
    default:
      return '';
  }
}

export function getSimpleResult(result: RoutineResult): StandardRoutineResult {
  assert(result);

  if (result.hasOwnProperty('simpleResult') &&
      result.simpleResult !== undefined) {
    return result.simpleResult as number;
    // Ideally we would just return assert(result.simpleResult) but enum
    // value 0 fails assert.
  }

  if (result.hasOwnProperty('powerResult')) {
    assert(result.powerResult);
    return result.powerResult.simpleResult as number;
  }
  assertNotReached();
}

/**
 * @fileoverview
 * 'routine-result-entry' shows the status of a single test routine or group.
 */

export class RoutineResultEntryElement extends PolymerElement {
  static get is(): 'routine-result-entry' {
    return 'routine-result-entry' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Added to support testing of announce behavior.
       */
      announcedText: {
        type: String,
        value: '',
      },

      item: {
        type: Object,
      },

      routineType: {
        type: String,
        computed: 'getRunningRoutineString(item.*)',
      },

      badgeType: {
        type: String,
        value: BadgeType.QUEUED,
      },

      badgeText: {
        type: String,
        value: '',
      },

      testCompleted: {
        type: Boolean,
        value: false,
      },

      hideVerticalLines: {
        type: Boolean,
        value: false,
      },

      usingRoutineGroups: {
        type: Boolean,
        value: false,
      },

    };
  }

  item: RoutineGroup|ResultStatusItem;
  hideVerticalLines: boolean;
  usingRoutineGroups: boolean;
  protected badgeType: BadgeType;
  protected badgeText: string;
  protected testCompleted: boolean;
  private announcedText: string;
  private routineType: string;


  static get observers(): string[] {
    return ['entryStatusChanged(item.*)'];
  }

  override connectedCallback(): void {
    super.connectedCallback();

    IronA11yAnnouncer.requestAvailability();
  }

  /**
   * Get the localized string name for the routine.
   */
  private getRunningRoutineString(): string {
    if (this.usingRoutineGroups) {
      assert(this.item instanceof RoutineGroup);
      return loadTimeData.getString(this.item.groupName);
    }

    assert(this.item instanceof ResultStatusItem);
    return loadTimeData.getStringF(
        'routineEntryText', getRoutineType(this.item.routine));
  }

  private entryStatusChanged(): void {
    switch (this.item.progress) {
      case ExecutionProgress.NOT_STARTED:
        this.setBadgeTypeAndText(
            BadgeType.QUEUED, loadTimeData.getString('testQueuedBadgeText'));
        break;
      case ExecutionProgress.RUNNING:
        this.setBadgeTypeAndText(
            BadgeType.RUNNING, loadTimeData.getString('testRunningBadgeText'));
        this.announceRoutineStatus();
        break;
      case ExecutionProgress.CANCELLED:
        this.setBadgeTypeAndText(
            BadgeType.STOPPED, loadTimeData.getString('testStoppedBadgeText'));
        break;
      case ExecutionProgress.COMPLETED:
        this.testCompleted = true;
        // Prevent warning state from being overridden.
        if (this.item instanceof RoutineGroup && this.item.inWarningState) {
          this.setBadgeTypeAndText(
              BadgeType.WARNING,
              loadTimeData.getString('testWarningBadgeText'));
          return;
        }

        let testPassed: boolean;
        if (this.usingRoutineGroups && this.item instanceof RoutineGroup) {
          testPassed = !this.item.failedTest;
        } else {
          assert(this.item instanceof ResultStatusItem);
          testPassed = !!this.item.result &&
              getSimpleResult(this.item.result) ===
                  StandardRoutineResult.kTestPassed;
        }

        const badgeType = testPassed ? BadgeType.SUCCESS : BadgeType.ERROR;
        const badgeText = loadTimeData.getString(
            testPassed ? 'testSucceededBadgeText' : 'testFailedBadgeText');
        this.setBadgeTypeAndText(badgeType, badgeText);
        if (!testPassed) {
          this.announceRoutineStatus();
        }
        break;
      case ExecutionProgress.SKIPPED:
        this.setBadgeTypeAndText(
            BadgeType.SKIPPED, loadTimeData.getString('testSkippedBadgeText'));
        break;
      case ExecutionProgress.WARNING:
        this.setBadgeTypeAndText(
            BadgeType.WARNING, loadTimeData.getString('testWarningBadgeText'));
        break;
      default:
        assertNotReached();
    }
  }

  private setBadgeTypeAndText(badgeType: BadgeType, badgeText: string): void {
    this.setProperties({badgeType: badgeType, badgeText: badgeText});
  }

  private announceRoutineStatus(): void {
    this.announcedText = this.routineType + ' - ' + this.badgeText;
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.announcedText,
      },
    }));
  }

  protected getLineClassName(num: number): string {
    if (!this.badgeType) {
      return '';
    }

    let lineColor = '';
    switch (this.badgeType) {
      case BadgeType.RUNNING:
      case BadgeType.SUCCESS:
        lineColor = 'green';
        break;
      case BadgeType.ERROR:
        lineColor = 'red';
        break;
      case BadgeType.WARNING:
        lineColor = 'yellow';
        break;
      case BadgeType.STOPPED:
      case BadgeType.QUEUED:
        return '';
    }
    return `line animation-${num} ${lineColor}`;
  }

  protected shouldHideLines(): boolean {
    return this.hideVerticalLines || !this.testCompleted;
  }

  protected computeFailedTestText(): string {
    if (!this.usingRoutineGroups) {
      return '';
    }
    assert(this.item instanceof RoutineGroup);
    if (!this.item.failedTest) {
      return '';
    }

    return getRoutineFailureMessage(this.item.failedTest);
  }

  getAnnouncedTextForTesting(): string {
    return this.announcedText;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RoutineResultEntryElement.is]: RoutineResultEntryElement;
  }
}

customElements.define(RoutineResultEntryElement.is, RoutineResultEntryElement);
