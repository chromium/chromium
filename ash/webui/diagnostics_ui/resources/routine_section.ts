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
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
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
  static get is(): 'routine-section' {
    return 'routine-section' as const;
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
      routineStartTimeMs: {
        type: Number,
        value: -1,
      },

      /**
       * Overall ExecutionProgress of the routine.
       */
      executionStatus: {
        type: Number,
        value: ExecutionProgress.NOT_STARTED,
      },

      /**
       * Name of currently running test
       */
      currentTestName: {
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

      powerRoutineResult: {
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

      badgeType: {
        type: String,
        value: BadgeType.RUNNING,
      },

      badgeText: {
        type: String,
        value: '',
      },

      statusText: {
        type: String,
        value: '',
      },

      isLoggedIn: {
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
      initialButtonText: {
        type: String,
        value: '',
        computed: 'getInitialButtonText(runTestsButtonText)',
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
        computed: 'getUsingRoutineGroupsVal(routines.*)',
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
  announcedText: string;
  currentTestName: string;
  private routineStartTimeMs: number;
  private executionStatus: ExecutionProgress;
  private powerRoutineResult: PowerRoutineResult;
  private badgeType: BadgeType;
  private badgeText: string;
  private statusText: string;
  private isLoggedIn: boolean;
  private bannerMessage: string;
  private initialButtonText: string;
  private executor: RoutineListExecutor|null = null;
  private failedTest: RoutineType|null = null;
  private hasTestFailure: boolean = false;
  private systemRoutineController: SystemRoutineControllerInterface|null = null;

  static get observers(): string[] {
    return [
      'routineStatusChanged(executionStatus, currentTestName,' +
          'additionalMessage)',
      'onActivePageChanged(isActive)',

    ];
  }

  override connectedCallback(): void {
    super.connectedCallback();

    IronA11yAnnouncer.requestAvailability();
  }

  private getInitialButtonText(buttonText: string): string {
    return this.initialButtonText || buttonText;
  }

  private getUsingRoutineGroupsVal(): boolean {
    if (this.routines.length === 0) {
      return false;
    }
    return this.routines[0] instanceof RoutineGroup;
  }

  private getResultListElem(): RoutineResultListElement {
    const routineResultList: RoutineResultListElement|null =
        this.shadowRoot!.querySelector('routine-result-list');
    assert(routineResultList);
    return routineResultList;
  }

  private async getSupportedRoutines(): Promise<RoutineType[]> {
    const supported =
        await this.systemRoutineController?.getSupportedRoutines();
    assert(supported);
    assert(isRoutineTypeArray(this.routines));
    const filteredRoutineTypes = this.routines.filter(
        (routine: RoutineType) => supported.routines.includes(routine));
    return filteredRoutineTypes;
  }

  private async getSupportedRoutineGroups(): Promise<RoutineGroup[]> {
    const supported =
        await this.systemRoutineController?.getSupportedRoutines();
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
    this.failedTest = null;

    this.systemRoutineController = getSystemRoutineController();
    const resultListElem = this.getResultListElem();
    const routines = this.usingRoutineGroups ?
        await this.getSupportedRoutineGroups() :
        await this.getSupportedRoutines();
    resultListElem.initializeTestRun(routines);

    // Expand result list by default.
    if (!this.shouldHideReportList()) {
      this.$.collapse.show();
    }

    if (this.bannerMessage) {
      this.showCautionBanner();
    }

    this.routineStartTimeMs = performance.now();

    // Set initial status badge text.
    this.setRunningStatusBadgeText();

    const remainingTimeUpdaterId =
        setInterval(() => this.setRunningStatusBadgeText(), 1000);
    assert(this.systemRoutineController);
    const executor = new RoutineListExecutor(this.systemRoutineController);
    this.executor = executor;
    if (!this.usingRoutineGroups) {
      assert(isRoutineTypeArray(routines));
      const status = await executor.runRoutines(
          routines,
          (routineStatus) =>
              this.handleRunningRoutineStatus(routineStatus, resultListElem));
      this.handleRoutinesCompletedStatus(status);
      clearInterval(remainingTimeUpdaterId);
      return;
    }
    assert(isRoutineGroupArray(routines));
    for (let i = 0; i < routines.length; i++) {
      const routineGroup = routines[i];
      const status = await executor.runRoutines(
          routineGroup.routines,
          (routineStatus) =>
              this.handleRunningRoutineStatus(routineStatus, resultListElem));
      const isLastRoutineGroup = i === routines.length - 1;
      if (isLastRoutineGroup) {
        this.handleRoutinesCompletedStatus(status);
        clearInterval(remainingTimeUpdaterId);
      }
    }
  }

  private announceRoutinesComplete(): void {
    this.announcedText = loadTimeData.getString('testOnRoutinesCompletedText');
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {
        text: this.announcedText,
      },
    }));
  }

  private handleRoutinesCompletedStatus(status: ExecutionProgress): void {
    this.executionStatus = status;
    this.testSuiteStatus = status === ExecutionProgress.CANCELLED ?
        TestSuiteStatus.NOT_RUNNING :
        TestSuiteStatus.COMPLETED;
    this.routineStartTimeMs = -1;
    this.runTestsButtonText = loadTimeData.getString('runAgainButtonText');
    this.getResultListElem().resetIgnoreStatusUpdatesFlag();
    this.cleanUp();
    if (status === ExecutionProgress.CANCELLED) {
      this.badgeText = loadTimeData.getString('testStoppedBadgeText');
    } else {
      this.badgeText = this.failedTest ?
          loadTimeData.getString('testFailedBadgeText') :
          loadTimeData.getString('testSucceededBadgeText');
      this.announceRoutinesComplete();
    }
  }

  private handleRunningRoutineStatus(
      status: ResultStatusItem,
      resultListElem: RoutineResultListElement): void {
    if (this.ignoreRoutineStatusUpdates) {
      return;
    }

    if (status.result && status.result.powerResult) {
      this.powerRoutineResult = status.result.powerResult;
    }

    if (status.result &&
        getSimpleResult(status.result) === StandardRoutineResult.kTestFailed &&
        !this.failedTest) {
      this.failedTest = status.routine;
    }

    // Execution progress is checked here to avoid overwriting
    // the test name shown in the status text.
    if (status.progress !== ExecutionProgress.CANCELLED) {
      this.currentTestName = getRoutineType(status.routine);
    }

    this.executionStatus = status.progress;

    resultListElem.onStatusUpdate.call(resultListElem, status);
  }

  private cleanUp(): void {
    if (this.executor) {
      this.executor.close();
      this.executor = null;
    }

    if (this.bannerMessage) {
      this.dismissCautionBanner();
    }

    this.systemRoutineController = null;
  }

  stopTests(): void {
    if (this.executor) {
      this.executor.cancel();
    }
  }

  private onToggleReportClicked(): void {
    // Toggle report list visibility
    this.$.collapse.toggle();
  }

  protected onLearnMoreClicked(): void {
    const baseSupportUrl =
        'https://support.google.com/chromebook?p=diagnostics_';
    assert(this.learnMoreLinkSection);

    window.open(baseSupportUrl + this.learnMoreLinkSection);
  }

  protected isResultButtonHidden(): boolean {
    return this.shouldHideReportList() ||
        this.executionStatus === ExecutionProgress.NOT_STARTED;
  }

  protected isLearnMoreHidden(): boolean {
    return !this.shouldHideReportList() || !this.isLoggedIn ||
        this.executionStatus !== ExecutionProgress.COMPLETED;
  }

  protected isStatusHidden(): boolean {
    return this.executionStatus === ExecutionProgress.NOT_STARTED;
  }

  /**
   * @param opened Whether the section is expanded or not.
   */
  protected getReportToggleButtonText(opened: boolean): string {
    return loadTimeData.getString(opened ? 'hideReportText' : 'seeReportText');
  }

  /**
   * Sets status texts for remaining runtime while the routine runs.
   */
  setRunningStatusBadgeText(): void {
    // Routines that are longer than 5 minutes are considered large
    const largeRoutine = this.routineRuntime >= 5;

    // Calculate time elapsed since the start of routine in minutes.
    const minsElapsed =
        (performance.now() - this.routineStartTimeMs) / 1000 / 60;
    let timeRemainingInMin = Math.ceil(this.routineRuntime - minsElapsed);

    if (largeRoutine && timeRemainingInMin <= 0) {
      this.statusText = loadTimeData.getString('routineRemainingMinFinalLarge');
      return;
    }

    // For large routines, round up to 5 minutes increments.
    if (largeRoutine && timeRemainingInMin % 5 !== 0) {
      timeRemainingInMin += (5 - timeRemainingInMin % 5);
    }

    this.badgeText = timeRemainingInMin <= 1 ?
        loadTimeData.getString('routineRemainingMinFinal') :
        loadTimeData.getStringF('routineRemainingMin', timeRemainingInMin);
  }

  protected routineStatusChanged(): void {
    switch (this.executionStatus) {
      case ExecutionProgress.NOT_STARTED:
        // Do nothing since status is hidden when tests have not been started.
        return;
      case ExecutionProgress.RUNNING:
        this.setBadgeAndStatusText(
            BadgeType.RUNNING,
            loadTimeData.getStringF(
                'routineNameText', this.currentTestName.toLowerCase()));
        return;
      case ExecutionProgress.CANCELLED:
        this.setBadgeAndStatusText(
            BadgeType.STOPPED,
            loadTimeData.getStringF('testCancelledText', this.currentTestName));
        return;
      case ExecutionProgress.COMPLETED:
        const isPowerRoutine = this.isPowerRoutine || this.powerRoutineResult;
        if (this.failedTest) {
          this.setBadgeAndStatusText(
              BadgeType.ERROR, loadTimeData.getString('testFailure'));
        } else {
          this.setBadgeAndStatusText(
              BadgeType.SUCCESS,
              isPowerRoutine ? this.getPowerRoutineString() :
                               loadTimeData.getString('testSuccess'));
        }
        return;
    }
    assertNotReached();
  }

  private getPowerRoutineString(): string {
    assert(!this.usingRoutineGroups);
    const stringId =
        (this.routines as RoutineType[]).includes(RoutineType.kBatteryCharge) ?
        'chargeTestResultText' :
        'dischargeTestResultText';
    const percentText = loadTimeData.getStringF(
        'percentageLabel',
        (this.powerRoutineResult?.percentChange || 0).toFixed(2));
    return loadTimeData.getStringF(
        stringId, percentText,
        this.powerRoutineResult?.timeElapsedSeconds || 0);
  }

  private setBadgeAndStatusText(badgeType: BadgeType, statusText: string):
      void {
    this.setProperties({
      badgeType: badgeType,
      statusText: statusText,
    });
  }

  protected isTestRunning(): boolean {
    return this.testSuiteStatus === TestSuiteStatus.RUNNING;
  }

  protected isRunTestsButtonHidden(): boolean {
    return this.isTestRunning() &&
        this.executionStatus === ExecutionProgress.RUNNING;
  }

  protected isStopTestsButtonHidden(): boolean {
    return this.executionStatus !== ExecutionProgress.RUNNING;
  }

  protected isRunTestsButtonDisabled(): boolean {
    return this.isTestRunning() || this.additionalMessage != '';
  }

  protected shouldHideReportList(): boolean {
    return this.routines.length < 2;
  }

  protected isAdditionalMessageHidden(): boolean {
    return this.additionalMessage == '';
  }

  private showCautionBanner(): void {
    this.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message: this.bannerMessage},
    }));
  }

  private dismissCautionBanner(): void {
    this.dispatchEvent(new CustomEvent(
        'dismiss-caution-banner', {bubbles: true, composed: true}));
  }

  private resetRoutineState(): void {
    this.setBadgeAndStatusText(BadgeType.QUEUED, '');
    this.badgeText = '';
    this.runTestsButtonText = this.initialButtonText;
    this.hasTestFailure = false;
    this.currentTestName = '';
    this.executionStatus = ExecutionProgress.NOT_STARTED;
    this.$.collapse.hide();
    this.ignoreRoutineStatusUpdates = false;
  }

  /**
   * If the page is active, check if we should run the routines
   * automatically, otherwise stop any running tests and reset to
   * the initial routine state.
   */
  private onActivePageChanged(): void {
    if (!this.isActive) {
      this.stopTests();
      this.resetRoutineState();
      return;
    }
  }

  protected isLearnMoreButtonHidden(): boolean {
    return !this.isLoggedIn || this.hideRoutineStatus;
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.cleanUp();
  }

  protected hideRoutineSection(): boolean {
    return this.routines.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RoutineSectionElement.is]: RoutineSectionElement;
  }
}

customElements.define(RoutineSectionElement.is, RoutineSectionElement);
