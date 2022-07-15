// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './routine_result_list.js';
import './text_badge.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PowerRoutineResult, RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {getSystemRoutineController} from './mojo_interface_provider.js';
import {RoutineGroup} from './routine_group.js';
import {ExecutionProgress, ResultStatusItem, RoutineListExecutor, TestSuiteStatus} from './routine_list_executor.js';
import {getRoutineType, getSimpleResult} from './routine_result_entry.js';
import {BadgeType} from './text_badge.js';

/**
 * @fileoverview
 * 'routine-section' has a button to run tests and displays their results. The
 * parent element eg. a CpuCard binds to the routines property to indicate
 * which routines this instance will run.
 */
Polymer({
  is: 'routine-section',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?RoutineListExecutor}
   */
  executor_: null,

  /**
   * Boolean whether last run had at least one failure,
   * @type {?RoutineType}
   * @private
   */
  failedTest_: null,

  /** @private {?SystemRoutineControllerInterface} */
  systemRoutineController_: null,

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

    /** @type {!Array<RoutineGroup|RoutineType>} */
    routines: {
      type: Array,
      value: () => [],
    },

    /**
     * Total time in minutes of estimate runtime based on routines array.
     * @type {number}
     */
    routineRuntime: {
      type: Number,
      value: 0,
    },

    /**
     * Timestamp of when routine test started execution in milliseconds.
     * @private {number}
     */
    routineStartTimeMs_: {
      type: Number,
      value: -1,
    },

    /**
     * Overall ExecutionProgress of the routine.
     * @type {!ExecutionProgress}
     * @private
     */
    executionStatus_: {
      type: Number,
      value: ExecutionProgress.kNotStarted,
    },

    /**
     * Name of currently running test
     * @private {string}
     */
    currentTestName_: {
      type: String,
      value: '',
    },

    /** @type {!TestSuiteStatus} */
    testSuiteStatus: {
      type: Number,
      value: TestSuiteStatus.kNotRunning,
      notify: true,
    },

    /** @type {boolean} */
    isPowerRoutine: {
      type: Boolean,
      value: false,
    },

    /** @private {?PowerRoutineResult} */
    powerRoutineResult_: {
      type: Object,
      value: null,
    },

    /** @type {string} */
    runTestsButtonText: {
      type: String,
      value: '',
    },

    /** @type {string} */
    additionalMessage: {
      type: String,
      value: '',
    },

    /** @type {string} */
    learnMoreLinkSection: {
      type: String,
      value: '',
    },

    /** @private {!BadgeType} */
    badgeType_: {
      type: String,
      value: BadgeType.RUNNING,
    },

    /** @private {string} */
    badgeText_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
    },

    /** @type {string} */
    bannerMessage: {
      type: Boolean,
      value: '',
    },

    /** @type {boolean} */
    isActive: {
      type: Boolean,
    },

    /**
     * Used to reset run button text to its initial state
     * when a navigation page change event occurs.
     *  @private {string}
     */
    initialButtonText_: {
      type: String,
      value: '',
      computed: 'getInitialButtonText_(runTestsButtonText)',
    },

    /** @type {boolean} */
    hideRoutineStatus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {boolean} */
    opened: {
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
      computed: 'getUsingRoutineGroupsVal_(routines.*)',
    },
  },

  observers: [
    'routineStatusChanged_(executionStatus_, currentTestName_,' +
        'additionalMessage)',
    'onActivePageChanged_(isActive)',
  ],

  /** @override */
  attached() {
    IronA11yAnnouncer.requestAvailability();
  },

  /**
   * @param {string} buttonText
   * @return {string}
   * @private
   */
  getInitialButtonText_(buttonText) {
    return this.initialButtonText_ || buttonText;
  },

  /**
   * @return {boolean}
   * @private
   */
  getUsingRoutineGroupsVal_() {
    if (this.routines.length === 0) {
      return false;
    }
    return this.routines[0] instanceof RoutineGroup;
  },

  /** @private */
  getResultListElem_() {
    return /** @type {!RoutineResultListElement} */ (
        this.$$('routine-result-list'));
  },


  /**
   *  @private
   *  @return {!Promise<!Array<!RoutineType>>}
   */
  async getSupportedRoutines_() {
    const supported =
        await this.systemRoutineController_.getSupportedRoutines();
    const filteredRoutineTypes = this.routines.filter(
        routine =>
            supported.routines.includes(/** @type {!RoutineType} */ (routine)));
    return filteredRoutineTypes;
  },

  /**
   *  @private
   *  @return {!Promise<!Array<!RoutineGroup>>}
   */
  async getSupportedRoutineGroups_() {
    const supported =
        await this.systemRoutineController_.getSupportedRoutines();
    const filteredRoutineGroups = [];
    for (const routineGroup of this.routines) {
      routineGroup.routines = routineGroup.routines.filter(
          routine => supported.routines.includes(routine));
      if (routineGroup.routines.length > 0) {
        filteredRoutineGroups.push(routineGroup.clone());
      }
    }
    return filteredRoutineGroups;
  },

  async runTests() {
    // Do not attempt to run tests when no routines available to run.
    if (this.routines.length === 0) {
      return;
    }
    this.testSuiteStatus = TestSuiteStatus.kRunning;
    this.failedTest_ = null;

    this.systemRoutineController_ = getSystemRoutineController();
    const resultListElem = this.getResultListElem_();
    const routines = this.usingRoutineGroups ?
        await this.getSupportedRoutineGroups_() :
        await this.getSupportedRoutines_();
    resultListElem.initializeTestRun(routines);

    // Expand result list by default.
    if (!this.shouldHideReportList_()) {
      this.$.collapse.show();
    }

    if (this.bannerMessage) {
      this.showCautionBanner_();
    }

    this.routineStartTimeMs_ = performance.now();

    // Set initial status badge text.
    this.setRunningStatusBadgeText_();

    const remainingTimeUpdaterId =
        setInterval(() => this.setRunningStatusBadgeText_(), 1000);
    const executor =
        new RoutineListExecutor(assert(this.systemRoutineController_));
    this.executor_ = executor;
    if (!this.usingRoutineGroups) {
      const status = await executor.runRoutines(
          routines,
          (routineStatus) =>
              this.handleRunningRoutineStatus_(routineStatus, resultListElem));
      this.handleRoutinesCompletedStatus_(status);
      clearInterval(remainingTimeUpdaterId);
      return;
    }

    for (let i = 0; i < routines.length; i++) {
      const routineGroup = routines[i];
      const status = await executor.runRoutines(
          routineGroup.routines,
          (routineStatus) =>
              this.handleRunningRoutineStatus_(routineStatus, resultListElem));
      const isLastRoutineGroup = i === routines.length - 1;
      if (isLastRoutineGroup) {
        this.handleRoutinesCompletedStatus_(status);
        clearInterval(remainingTimeUpdaterId);
      }
    }
  },

  /** @private */
  announceRoutinesComplete_() {
    this.announcedText_ =
        loadTimeData.getString('testOnRoutinesCompletedText');
    this.fire('iron-announce', {text: `${this.announcedText_}`});
  },

  /** @param {!ExecutionProgress} status */
  handleRoutinesCompletedStatus_(status) {
    this.executionStatus_ = status;
    this.testSuiteStatus = status === ExecutionProgress.kCancelled ?
        TestSuiteStatus.kNotRunning :
        TestSuiteStatus.kCompleted;
    this.routineStartTimeMs_ = -1;
    this.runTestsButtonText = loadTimeData.getString('runAgainButtonText');
    this.getResultListElem_().resetIgnoreStatusUpdatesFlag();
    this.cleanUp_();
    if (status === ExecutionProgress.kCancelled) {
      this.badgeText_ = loadTimeData.getString('testStoppedBadgeText');
    } else {
      this.badgeText_ = this.failedTest_ ?
          loadTimeData.getString('testFailedBadgeText') :
          loadTimeData.getString('testSucceededBadgeText');
      this.announceRoutinesComplete_();
    }
  },

  /**
   * @param {!ResultStatusItem} status
   * @param {!RoutineResultListElement} resultListElem
   * @private
   */
  handleRunningRoutineStatus_(status, resultListElem) {
    if (this.ignoreRoutineStatusUpdates) {
      return;
    }

    if (status.result && status.result.powerResult) {
      this.powerRoutineResult_ = status.result.powerResult;
    }

    if (status.result &&
        getSimpleResult(status.result) === StandardRoutineResult.kTestFailed &&
        !this.failedTest_) {
      this.failedTest_ = status.routine;
    }

    // Execution progress is checked here to avoid overwriting
    // the test name shown in the status text.
    if (status.progress !== ExecutionProgress.kCancelled) {
      this.currentTestName_ = getRoutineType(status.routine);
    }

    this.executionStatus_ = status.progress;

    resultListElem.onStatusUpdate.call(resultListElem, status);
  },

  /** @private */
  cleanUp_() {
    if (this.executor_) {
      this.executor_.close();
      this.executor_ = null;
    }

    if (this.bannerMessage) {
      this.dismissCautionBanner_();
    }

    this.systemRoutineController_ = null;
  },

  stopTests() {
    if (this.executor_) {
      this.executor_.cancel();
    }
  },

  /** @private */
  onToggleReportClicked_() {
    // Toggle report list visibility
    this.$.collapse.toggle();
  },

  /** @protected */
  onLearnMoreClicked_() {
    const baseSupportUrl =
        'https://support.google.com/chromebook?p=diagnostics_';
    assert(this.learnMoreLinkSection);

    window.open(baseSupportUrl + this.learnMoreLinkSection);
  },

  /** @protected */
  isResultButtonHidden_() {
    return this.shouldHideReportList_() ||
        this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /** @protected */
  isLearnMoreHidden_() {
    return !this.shouldHideReportList_() || !this.isLoggedIn_ ||
        this.executionStatus_ !== ExecutionProgress.kCompleted;
  },

  /** @protected */
  isStatusHidden_() {
    return this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /**
   * @param {boolean} opened Whether the section is expanded or not.
   * @return {string} button text.
   * @protected
   */
  getReportToggleButtonText_(opened) {
    return loadTimeData.getString(opened ? 'hideReportText' : 'seeReportText');
  },

  /**
   * Sets status texts for remaining runtime while the routine runs.
   * @private
   */
  setRunningStatusBadgeText_() {
    // Routines that are longer than 5 minutes are considered large
    const largeRoutine = this.routineRuntime >= 5;

    // Calculate time elapsed since the start of routine in minutes.
    const minsElapsed =
        (performance.now() - this.routineStartTimeMs_) / 1000 / 60;
    let timeRemainingInMin = Math.ceil(this.routineRuntime - minsElapsed);

    if (largeRoutine && timeRemainingInMin <= 0) {
      this.statusText_ =
          loadTimeData.getString('routineRemainingMinFinalLarge');
      return;
    }

    // For large routines, round up to 5 minutes increments.
    if (largeRoutine && timeRemainingInMin % 5 !== 0) {
      timeRemainingInMin += (5 - timeRemainingInMin % 5);
    }

    this.badgeText_ = timeRemainingInMin <= 1 ?
        loadTimeData.getString('routineRemainingMinFinal') :
        loadTimeData.getStringF('routineRemainingMin', timeRemainingInMin);
  },

  /** @protected */
  routineStatusChanged_() {
    switch (this.executionStatus_) {
      case ExecutionProgress.kNotStarted:
        // Do nothing since status is hidden when tests have not been started.
        break;
      case ExecutionProgress.kRunning:
        this.setBadgeAndStatusText_(
            BadgeType.RUNNING,
            loadTimeData.getStringF(
                'routineNameText', this.currentTestName_.toLowerCase()));
        break;
      case ExecutionProgress.kCancelled:
        this.setBadgeAndStatusText_(
            BadgeType.STOPPED,
            loadTimeData.getStringF(
                'testCancelledText', this.currentTestName_));
        break;
      case ExecutionProgress.kCompleted:
        const isPowerRoutine = this.isPowerRoutine || this.powerRoutineResult_;
        if (this.failedTest_) {
          this.setBadgeAndStatusText_(
              BadgeType.ERROR, loadTimeData.getString('testFailure'));
        } else {
          this.setBadgeAndStatusText_(
              BadgeType.SUCCESS,
              isPowerRoutine ? this.getPowerRoutineString_() :
                               loadTimeData.getString('testSuccess'));
        }
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getPowerRoutineString_() {
    assert(!this.usingRoutineGroups);
    const stringId = this.routines.includes(RoutineType.kBatteryCharge) ?
        'chargeTestResultText' :
        'dischargeTestResultText';
    const percentText = loadTimeData.getStringF(
        'percentageLabel', this.powerRoutineResult_.percentChange.toFixed(2));
    return loadTimeData.getStringF(
        stringId, percentText, this.powerRoutineResult_.timeElapsedSeconds);
  },

  /**
   * @param {!BadgeType} badgeType
   * @param {string} statusText
   * @private
   */
  setBadgeAndStatusText_(badgeType, statusText) {
    this.setProperties({
      badgeType_: badgeType,
      statusText_: statusText,
    });
  },

  /**
   * @protected
   * @return {boolean}
   */
  isTestRunning_() {
    return this.testSuiteStatus === TestSuiteStatus.kRunning;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isRunTestsButtonHidden_() {
    return this.isTestRunning_() &&
        this.executionStatus_ === ExecutionProgress.kRunning;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isStopTestsButtonHidden_() {
    return this.executionStatus_ !== ExecutionProgress.kRunning;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isRunTestsButtonDisabled_() {
    return this.isTestRunning_() || this.additionalMessage != '';
  },

  /**
   * @protected
   * @return {boolean}
   */
  shouldHideReportList_() {
    return this.routines.length < 2;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isAdditionalMessageHidden_() {
    return this.additionalMessage == '';
  },

  /**
   * @private
   */
  showCautionBanner_() {
    this.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message: this.bannerMessage},
    }));
  },

  /** @private */
  dismissCautionBanner_() {
    this.dispatchEvent(new CustomEvent(
        'dismiss-caution-banner', {bubbles: true, composed: true}));
  },

  /** @private */
  resetRoutineState_() {
    this.setBadgeAndStatusText_(BadgeType.QUEUED, '');
    this.badgeText_ = '';
    this.runTestsButtonText = this.initialButtonText_;
    this.hasTestFailure_ = false;
    this.currentTestName_ = '';
    this.executionStatus_ = ExecutionProgress.kNotStarted;
    this.$.collapse.hide();
    this.ignoreRoutineStatusUpdates = false;
  },

  /**
   * If the page is active, check if we should run the routines
   * automatically, otherwise stop any running tests and reset to
   * the initial routine state.
   * @private
   */
  onActivePageChanged_() {
    if (!this.isActive) {
      this.stopTests();
      this.resetRoutineState_();
      return;
    }
  },

  /**
   * @protected
   * @return {boolean}
   */
  isLearnMoreButtonHidden_() {
    return !this.isLoggedIn_ || this.hideRoutineStatus;
  },

  /** @override */
  detached() {
    this.cleanUp_();
  },

  /** @override */
  created() {},

  /**
   * @protected
   * @return {boolean}
   */
  hideRoutineSection() {
    return this.routines.length === 0;
  },
});
