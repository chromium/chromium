// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './text_badge.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RoutineResult, RoutineType, StandardRoutineResult} from './diagnostics_types.js';
import {getRoutineFailureMessage} from './diagnostics_utils.js';
import {RoutineGroup} from './routine_group.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {BadgeType} from './text_badge.js';

/**
 * Resolves a routine name to its corresponding localized string name.
 * @param {!RoutineType} routineType
 * @return {string}
 */
export function getRoutineType(routineType) {
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
      // Values should always be found in the enum.
      assert(false);
      return '';
  }
}

/**
 * @param {!RoutineResult} result
 * @return {?StandardRoutineResult}
 */
export function getSimpleResult(result) {
  if (!result) {
    return null;
  }

  if (result.hasOwnProperty('simpleResult')) {
    // Ideally we would just return assert(result.simpleResult) but enum
    // value 0 fails assert.
    return /** @type {!StandardRoutineResult} */ (result.simpleResult);
  }

  if (result.hasOwnProperty('powerResult')) {
    return /** @type {!StandardRoutineResult} */ (
        result.powerResult.simpleResult);
  }

  assertNotReached();
  return null;
}

/**
 * @fileoverview
 * 'routine-result-entry' shows the status of a single test routine or group.
 */
Polymer({
  is: 'routine-result-entry',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Added to support testing of announce behavior.
     * @private
     * @type {string}
     */
    announcedText_: {
      type: String,
      value: '',
    },

    /** @type {RoutineGroup|ResultStatusItem} */
    item: {
      type: Object,
    },

    /** @private */
    routineType_: {
      type: String,
      computed: 'getRunningRoutineString_(item.*)',
    },

    /** @protected {!BadgeType} */
    badgeType_: {
      type: String,
      value: BadgeType.QUEUED,
    },

    /** @protected {string} */
    badgeText_: {
      type: String,
      value: '',
    },

    /** @protected {boolean} */
    testCompleted_: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    hideVerticalLines: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    usingRoutineGroups: {
      type: Boolean,
      value: false,
    },
  },

  observers: ['entryStatusChanged_(item.*)'],

  /** @override */
  attached() {
    IronA11yAnnouncer.requestAvailability();
  },

  /**
   * Get the localized string name for the routine.
   * @return {string}
   */
  getRunningRoutineString_() {
    if (this.usingRoutineGroups) {
      return loadTimeData.getString(this.item.groupName);
    }

    return loadTimeData.getStringF(
        'routineEntryText', getRoutineType(this.item.routine));
  },

  /**
   * @private
   */
  entryStatusChanged_() {
    switch (this.item.progress) {
      case ExecutionProgress.kNotStarted:
        this.setBadgeTypeAndText_(
            BadgeType.QUEUED, loadTimeData.getString('testQueuedBadgeText'));
        break;
      case ExecutionProgress.kRunning:
        this.setBadgeTypeAndText_(
            BadgeType.RUNNING, loadTimeData.getString('testRunningBadgeText'));
        this.announceRoutineStatus_();
        break;
      case ExecutionProgress.kCancelled:
        this.setBadgeTypeAndText_(
            BadgeType.STOPPED, loadTimeData.getString('testStoppedBadgeText'));
        break;
      case ExecutionProgress.kCompleted:
        this.testCompleted_ = true;
        // Prevent warning state from being overridden.
        if (this.item.inWarningState) {
          this.setBadgeTypeAndText_(
              BadgeType.WARNING,
              loadTimeData.getString('testWarningBadgeText'));
          return;
        }

        const testPassed = this.usingRoutineGroups ?
            !this.item.failedTest :
            (this.item.result &&
             getSimpleResult(this.item.result) ===
                 StandardRoutineResult.kTestPassed);
        const badgeType = testPassed ? BadgeType.SUCCESS : BadgeType.ERROR;
        const badgeText = loadTimeData.getString(
            testPassed ? 'testSucceededBadgeText' : 'testFailedBadgeText');
        this.setBadgeTypeAndText_(badgeType, badgeText);
        if (!testPassed) {
          this.announceRoutineStatus_();
        }
        break;
      case ExecutionProgress.kSkipped:
        this.setBadgeTypeAndText_(
            BadgeType.SKIPPED, loadTimeData.getString('testSkippedBadgeText'));
        break;
      case ExecutionProgress.kWarning:
        this.setBadgeTypeAndText_(
            BadgeType.WARNING, loadTimeData.getString('testWarningBadgeText'));
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * @param {!BadgeType} badgeType
   * @param {string} badgeText
   * @private
   */
  setBadgeTypeAndText_(badgeType, badgeText) {
    this.setProperties({badgeType_: badgeType, badgeText_: badgeText});
  },

  /** @override */
  created() {},

  /** @private */
  announceRoutineStatus_() {
    this.announcedText_ = this.routineType_ + ' - ' + this.badgeText_;
    this.fire('iron-announce', {text: `${this.announcedText_}`});
  },

  /**
   * @protected
   * @return {string}
   */
  getLineClassName_(num) {
    if (!this.badgeType_) {
      return '';
    }

    let lineColor = '';
    switch (this.badgeType_) {
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
  },

  /**
   * @protected
   * @return {boolean}
   */
  shouldHideLines_() {
    return this.hideVerticalLines || !this.testCompleted_;
  },

  /**
   * @protected
   * @return {string}
   */
  computeFailedTestText_() {
    if (!this.usingRoutineGroups || !this.item.failedTest) {
      return '';
    }

    return getRoutineFailureMessage(this.item.failedTest);
  },
});
