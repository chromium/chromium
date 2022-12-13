// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './diagnostics_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './routine_result_list.js';
import './text_badge.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSystemRoutineController} from './mojo_interface_provider.js';
import {RoutineGroup} from './routine_group.js';
import {ExecutionProgress, ResultStatusItem, RoutineListExecutor, TestSuiteStatus} from './routine_list_executor.js';
import {getRoutineType, getSimpleResult} from './routine_result_entry.js';
import {isRoutineGroupArray, isRoutineTypeArray, RoutineResultListElement} from './routine_result_list.js';
import {getTemplate} from './routine_section.html.js';
import {PowerRoutineResult, RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './system_routine_controller.mojom-webui.js';
import {BadgeType} from './text_badge.js';

export type Routines = RoutineGroup[]|RoutineType[];

export interface RoutineSectionElement {
  $: {
    collapse: IronCollapseElement,
  };
}

/**
 * @fileoverview
 * 'routine-section' has a button to run tests and displays their results. The
 * parent element eg. a CpuCard binds to the routines property to indicate
 * which routines this instance will run.
 */

const RoutineSectionElementBase = I18nMixin(PolymerElement);

export class RoutineSectionElement extends RoutineSectionElementBase {
  static get is() {
    return 'routine-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Added to support testing of announce behavior.
       */
      announcedText_: {
        type: String,
        value: '',
      },

      routines: {
        type: Array,
        value: () => [],
      },

      /**
       * Total time in minutes of estimate runtime based on routines array.
       */
      routineRuntime: {
        type: Number,
        value: 0,
      },

      /**
       * Timestamp of when routine test started execution in milliseconds.
       */
      routineStartTimeMs_: {
        type: Number,
        value: -1,
      },

      /**
       * Overall ExecutionProgress of the routine.
       */
      executionStatus_: {
        type: Number,
        value: ExecutionProgress.NOT_STARTED,
      },

      /**
       * Name of currently running test
       */
      currentTestName_: {
        type: String,
        value: '',
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
        notify: true,
      },

      isPowerRoutine: {
        type: Boolean,
        value: false,
      },

      powerRoutineResult_: {
        type: Object,
        value: null,
      },

      runTestsButtonText: {
        type: String,
        value: '',
      },

      additionalMessage: {
        type: String,
        value: '',
      },

      learnMoreLinkSection: {
        type: String,
        value: '',
      },

      badgeType_: {
        type: String,
        value: BadgeType.RUNNING,
      },

      badgeText_: {
        type: String,
        value: '',
      },

      statusText_: {
        type: String,
        value: '',
      },

      isLoggedIn_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      bannerMessage: {
        type: Boolean,
        value: '',
      },

      isActive: {
        type: Boolean,
      },

      /**
       * Used to reset run button text to its initial state
       * when a navigation page change event occurs.
       */
      initialButtonText_: {
        type: String,
        value: '',
        computed: 'getInitialButtonText_(runTestsButtonText)',
      },

      hideRoutineStatus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      opened: {
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
        computed: 'getUsingRoutineGroupsVal_(routines.*)',
      },

    };
  }

  routines: Routines;
  routineRuntime: number;
  runTestsButtonText: string;
  additionalMessage: string;
  learnMoreLinkSection: string;
  testSuiteStatus: TestSuiteStatus;
  isPowerRoutine: boolean;
  isActive: boolean;
  hideRoutineStatus: boolean;
  opened: boolean;
  hideVerticalLines: boolean;
  usingRoutineGroups: boolean;
  ignoreRoutineStatusUpdates: boolean;
  private announcedText_: string;
  private routineStartTimeMs_: number;
  private executionStatus_: ExecutionProgress;
  private currentTestName_: string;
  private powerRoutineResult_: PowerRoutineResult;
  private badgeType_: BadgeType;
  private badgeText_: string;
  private statusText_: string;
  private isLoggedIn_: boolean;
  private bannerMessage: string;
  private initialButtonText_: string;
  private executor_: RoutineListExecutor|null = null;
  private failedTest_: RoutineType|null = null;
  private hasTestFailure_: boolean = false;
  private systemRoutineController_: SystemRoutineControllerInterface|null =
      null;

  static get observers() {
    return [
      'routineStatusChanged_(executionStatus_, currentTestName_,' +
          'additionalMessage)',
      'onActivePageChanged_(isActive)',

    ];
  }

  override connectedCallback() {
    super.connectedCallback();

    IronA11yAnnouncer.requestAvailability();
  }

  private getInitialButtonText_(buttonText: string): string {
    return this.initialButtonText_ || buttonText;
  }

  private getUsingRoutineGroupsVal_(): boolean {
    if (this.routines.length === 0) {
      return false;
    }
    return this.routines[0] instanceof RoutineGroup;
  }

  private getResultListElem_(): RoutineResultListElement {
    const routineResultList: RoutineResultListElement|null =
        this.shadowRoot!.querySelector('routine-result-list');
    assert(routineResultList);
    return routineResultList;
  }

  private async getSupportedRoutines_(): Promise<RoutineType[]> {
    const supported =
        await this.systemRoutineController_?.getSupportedRoutines();
    assert(supported);
    assert(isRoutineTypeArray(this.routines));
    const filteredRoutineTypes = this.routines.filter(
        (routine: RoutineType) => supported.routines.includes(routine));
    return filteredRoutineTypes;
  }

  private async getSupportedRoutineGroups_(): Promise<RoutineGroup[]> {
    const supported =
        await this.systemRoutineController_?.getSupportedRoutines();
    assert(supported);
    const filteredRoutineGroups: RoutineGroup[] = [];
    assert(isRoutineGroupArray(this.routines));
    for (const routineGroup of this.routines) {
      routineGroup.routines = routineGroup.routines.filter(
          routine => supported.routines.includes(routine));
      if (routineGroup.routines.length > 0) {
        filteredRoutineGroups.push(routineGroup.clone());
      }
    }
    return filteredRoutineGroups;
  }

  async runTests(): Promise<void> {
    // Do not attempt to run tests when no routines available to run.
    if (this.routines.length === 0) {
      return;
    }
    this.testSuiteStatus = TestSuiteStatus.RUNNING;
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
    assert(this.systemRoutineController_);
    const executor = new RoutineListExecutor(this.systemRoutineController_);
    this.executor_ = executor;
    if (!this.usingRoutineGroups) {
      assert(isRoutineTypeArray(routines));
      const status = await executor.runRoutines(
          routines,
          (routineStatus) =>
              this.handleRunningRoutineStatus_(routineStatus, resultListElem));
      this.handleRoutinesCompletedStatus_(status);
      clearInterval(remainingTimeUpdaterId);
      return;
    }
    assert(isRoutineGroupArray(routines));
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
  }

  private announceRoutinesComplete_(): void {
    this.announcedText_ = loadTimeData.getString('testOnRoutinesCompletedText');
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.announcedText_,
      },
    }));
  }

  private handleRoutinesCompletedStatus_(status: ExecutionProgress): void {
    this.executionStatus_ = status;
    this.testSuiteStatus = status === ExecutionProgress.CANCELLED ?
        TestSuiteStatus.NOT_RUNNING :
        TestSuiteStatus.COMPLETED;
    this.routineStartTimeMs_ = -1;
    this.runTestsButtonText = loadTimeData.getString('runAgainButtonText');
    this.getResultListElem_().resetIgnoreStatusUpdatesFlag();
    this.cleanUp_();
    if (status === ExecutionProgress.CANCELLED) {
      this.badgeText_ = loadTimeData.getString('testStoppedBadgeText');
    } else {
      this.badgeText_ = this.failedTest_ ?
          loadTimeData.getString('testFailedBadgeText') :
          loadTimeData.getString('testSucceededBadgeText');
      this.announceRoutinesComplete_();
    }
  }

  private handleRunningRoutineStatus_(
      status: ResultStatusItem,
      resultListElem: RoutineResultListElement): void {
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
    if (status.progress !== ExecutionProgress.CANCELLED) {
      this.currentTestName_ = getRoutineType(status.routine);
    }

    this.executionStatus_ = status.progress;

    resultListElem.onStatusUpdate.call(resultListElem, status);
  }

  private cleanUp_(): void {
    if (this.executor_) {
      this.executor_.close();
      this.executor_ = null;
    }

    if (this.bannerMessage) {
      this.dismissCautionBanner_();
    }

    this.systemRoutineController_ = null;
  }

  stopTests(): void {
    if (this.executor_) {
      this.executor_.cancel();
    }
  }

  private onToggleReportClicked_(): void {
    // Toggle report list visibility
    this.$.collapse.toggle();
  }

  protected onLearnMoreClicked_(): void {
    const baseSupportUrl =
        'https://support.google.com/chromebook?p=diagnostics_';
    assert(this.learnMoreLinkSection);

    window.open(baseSupportUrl + this.learnMoreLinkSection);
  }

  protected isResultButtonHidden_(): boolean {
    return this.shouldHideReportList_() ||
        this.executionStatus_ === ExecutionProgress.NOT_STARTED;
  }

  protected isLearnMoreHidden_(): boolean {
    return !this.shouldHideReportList_() || !this.isLoggedIn_ ||
        this.executionStatus_ !== ExecutionProgress.COMPLETED;
  }

  protected isStatusHidden_(): boolean {
    return this.executionStatus_ === ExecutionProgress.NOT_STARTED;
  }

  /**
   * @param opened Whether the section is expanded or not.
   */
  protected getReportToggleButtonText_(opened: boolean): string {
    return loadTimeData.getString(opened ? 'hideReportText' : 'seeReportText');
  }

  /**
   * Sets status texts for remaining runtime while the routine runs.
   */
  private setRunningStatusBadgeText_(): void {
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
  }

  protected routineStatusChanged_(): void {
    switch (this.executionStatus_) {
      case ExecutionProgress.NOT_STARTED:
        // Do nothing since status is hidden when tests have not been started.
        return;
      case ExecutionProgress.RUNNING:
        this.setBadgeAndStatusText_(
            BadgeType.RUNNING,
            loadTimeData.getStringF(
                'routineNameText', this.currentTestName_.toLowerCase()));
        return;
      case ExecutionProgress.CANCELLED:
        this.setBadgeAndStatusText_(
            BadgeType.STOPPED,
            loadTimeData.getStringF(
                'testCancelledText', this.currentTestName_));
        return;
      case ExecutionProgress.COMPLETED:
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
        return;
    }
    assertNotReached();
  }

  private getPowerRoutineString_(): string {
    assert(!this.usingRoutineGroups);
    const stringId =
        (this.routines as RoutineType[]).includes(RoutineType.kBatteryCharge) ?
        'chargeTestResultText' :
        'dischargeTestResultText';
    const percentText = loadTimeData.getStringF(
        'percentageLabel', this.powerRoutineResult_.percentChange.toFixed(2));
    return loadTimeData.getStringF(
        stringId, percentText, this.powerRoutineResult_.timeElapsedSeconds);
  }

  private setBadgeAndStatusText_(badgeType: BadgeType, statusText: string):
      void {
    this.setProperties({
      badgeType_: badgeType,
      statusText_: statusText,
    });
  }

  protected isTestRunning_(): boolean {
    return this.testSuiteStatus === TestSuiteStatus.RUNNING;
  }

  protected isRunTestsButtonHidden_(): boolean {
    return this.isTestRunning_() &&
        this.executionStatus_ === ExecutionProgress.RUNNING;
  }

  protected isStopTestsButtonHidden_(): boolean {
    return this.executionStatus_ !== ExecutionProgress.RUNNING;
  }

  protected isRunTestsButtonDisabled_(): boolean {
    return this.isTestRunning_() || this.additionalMessage != '';
  }

  protected shouldHideReportList_(): boolean {
    return this.routines.length < 2;
  }

  protected isAdditionalMessageHidden_(): boolean {
    return this.additionalMessage == '';
  }

  private showCautionBanner_(): void {
    this.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message: this.bannerMessage},
    }));
  }

  private dismissCautionBanner_(): void {
    this.dispatchEvent(new CustomEvent(
        'dismiss-caution-banner', {bubbles: true, composed: true}));
  }

  private resetRoutineState_(): void {
    this.setBadgeAndStatusText_(BadgeType.QUEUED, '');
    this.badgeText_ = '';
    this.runTestsButtonText = this.initialButtonText_;
    this.hasTestFailure_ = false;
    this.currentTestName_ = '';
    this.executionStatus_ = ExecutionProgress.NOT_STARTED;
    this.$.collapse.hide();
    this.ignoreRoutineStatusUpdates = false;
  }

  /**
   * If the page is active, check if we should run the routines
   * automatically, otherwise stop any running tests and reset to
   * the initial routine state.
   */
  private onActivePageChanged_(): void {
    if (!this.isActive) {
      this.stopTests();
      this.resetRoutineState_();
      return;
    }
  }

  protected isLearnMoreButtonHidden_(): boolean {
    return !this.isLoggedIn_ || this.hideRoutineStatus;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.cleanUp_();
  }

  protected hideRoutineSection(): boolean {
    return this.routines.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'routine-section': RoutineSectionElement;
  }
}

customElements.define(RoutineSectionElement.is, RoutineSectionElement);
