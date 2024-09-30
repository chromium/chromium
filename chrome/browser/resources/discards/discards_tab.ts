// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {boolToString, durationToString, getOrCreateDetailsProvider} from './discards.js';
import type {DetailsProviderRemote, TabDiscardsInfo} from './discards.mojom-webui.js';
import {LifecycleUnitVisibility} from './discards.mojom-webui.js';
import {getTemplate} from './discards_tab.html.js';
import {LifecycleUnitDiscardReason, LifecycleUnitLoadingState, LifecycleUnitState} from './lifecycle_unit_state.mojom-webui.js';
import {SortedTableMixin} from './sorted_table_mixin.js';

interface DictType {
  [key: string]: (boolean|number|string);
}

/**
 * Compares two TabDiscardsInfos based on the data in the provided sort-key.
 * @param sortKey The key of the sort. See the "data-sort-key"
 *     attribute of the table headers for valid sort-keys.
 * @param a The first value being compared.
 * @param b The second value being compared.
 * @return A negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
export function compareTabDiscardsInfos(
    sortKey: string, a: DictType, b: DictType): number {
  let val1 = a[sortKey];
  let val2 = b[sortKey];

  // Compares strings.
  if (sortKey === 'title' || sortKey === 'tabUrl') {
    val1 = (val1 as string).toLowerCase();
    val2 = (val2 as string).toLowerCase();
    if (val1 === val2) {
      return 0;
    }
    return val1 > val2 ? 1 : -1;
  }

  // Compares boolean fields.
  if (['isAutoDiscardable'].includes(sortKey)) {
    if (val1 === val2) {
      return 0;
    }
    return val1 ? 1 : -1;
  }

  // Compare lifecycle state. This is actually a compound key.
  if (sortKey === 'state') {
    // If the keys are discarding state, then break ties using the discard
    // reason.
    if (val1 === val2 && val1 === LifecycleUnitState.DISCARDED) {
      val1 = a['discardReason'];
      val2 = b['discardReason'];
    }
    return (val1 as LifecycleUnitState) - (val2 as LifecycleUnitState);
  }

  // Compares numeric fields.
  // NOTE: visibility, loadingState and state are represented as a numeric
  // value.
  if ([
        'visibility',
        'loadingState',
        'discardCount',
        'utilityRank',
        'lastActiveSeconds',
        'siteEngagementScore',
      ].includes(sortKey)) {
    return (val1 as number) - (val2 as number);
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
}

const DiscardsTabElementBase = SortedTableMixin(PolymerElement);

class DiscardsTabElement extends DiscardsTabElementBase {
  static get is() {
    return 'discards-tab';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tabInfos_: Array,
      isPerformanceInterventionDemoModeEnabled_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isPerformanceInterventionDemoModeEnabled');
        },
      },
    };
  }

  private tabInfos_: TabDiscardsInfo[];
  private isPerformanceInterventionDemoModeEnabled_: boolean;

  /** The current update timer if any. */
  private updateTimer_: number = 0;

  private discardsDetailsProvider_: DetailsProviderRemote|null = null;

  override connectedCallback() {
    this.setSortKey('utilityRank');
    this.discardsDetailsProvider_ = getOrCreateDetailsProvider();

    this.updateTable_();
  }

  /**
   * Returns a sort function to compare tab infos based on the provided sort
   * key and a boolean reverse flag.
   * @param sortKey The sort key for the  returned function.
   * @param sortReverse True if sorting is reversed.
   * @return A comparison function that compares two tab infos, returns
   *     negative number if a < b, 0 if a === b, and a positive
   *     number if a > b.
   * @private
   */
  private computeSortFunction_(sortKey: string, sortReverse: boolean):
      (a: DictType, b: DictType) => number {
    // Polymer 2.0 may invoke multi-property observers before all properties
    // are defined.
    if (!sortKey) {
      return (_a: DictType, _b: DictType) => 0;
    }

    return function(a: DictType, b: DictType) {
      const comp = compareTabDiscardsInfos(sortKey, a, b);
      return sortReverse ? -comp : comp;
    };
  }

  /**
   * Returns a string representation of a visibility enum value for display in
   * a table.
   * @param visibility A visibility value.
   * @return A string representation of the visibility.
   */
  private visibilityToString_(visibility: LifecycleUnitVisibility): string {
    switch (visibility) {
      case LifecycleUnitVisibility.HIDDEN:
        return 'hidden';
      case LifecycleUnitVisibility.OCCLUDED:
        return 'occluded';
      case LifecycleUnitVisibility.VISIBLE:
        return 'visible';
    }
  }

  /**
   * Returns a string representation of a loading state enum value for display
   * in a table.
   * @param loadingState A loading state value.
   * @return A string representation of the loading state.
   */
  private loadingStateToString_(loadingState: LifecycleUnitLoadingState):
      string {
    switch (loadingState) {
      case LifecycleUnitLoadingState.UNLOADED:
        return 'unloaded';
      case LifecycleUnitLoadingState.LOADING:
        return 'loading';
      case LifecycleUnitLoadingState.LOADED:
        return 'loaded';
    }
  }

  /**
   * Returns a string representation of a discard reason.
   * @param reason The discard reason.
   * @return A string representation of the discarding reason.
   */
  private discardReasonToString_(reason: LifecycleUnitDiscardReason): string {
    switch (reason) {
      case LifecycleUnitDiscardReason.EXTERNAL:
        return 'external';
      case LifecycleUnitDiscardReason.URGENT:
        return 'urgent';
      case LifecycleUnitDiscardReason.PROACTIVE:
        return 'proactive';
      case LifecycleUnitDiscardReason.SUGGESTED:
        return 'suggested';
    }
  }

  /**
   * Returns a string representation of a lifecycle state.
   * @param state The lifecycle state.
   * @param reason The discard reason. This
   *     is only used if the state is discard related.
   * @param visibility A visibility value.
   * @param hasFocus Whether or not the tab has input focus.
   * @param stateChangeTime Delta between Unix Epoch and the time at
   *     which the lifecycle state has changed.
   * @return A string representation of the lifecycle state,
   *     augmented with the discard reason if appropriate.
   */
  private lifecycleStateToString_(
      state: LifecycleUnitState, reason: LifecycleUnitDiscardReason,
      visibility: LifecycleUnitVisibility, hasFocus: boolean,
      stateChangeTime: TimeDelta): string {
    function pageLifecycleStateFromVisibilityAndFocus(): string {
      switch (visibility) {
        case LifecycleUnitVisibility.HIDDEN:
        case LifecycleUnitVisibility.OCCLUDED:
          // An occluded page is also considered hidden.
          return 'hidden';
        case LifecycleUnitVisibility.VISIBLE:
          return hasFocus ? 'active' : 'passive';
      }
    }

    switch (state) {
      case LifecycleUnitState.ACTIVE:
        return pageLifecycleStateFromVisibilityAndFocus();
      case LifecycleUnitState.THROTTLED:
        return pageLifecycleStateFromVisibilityAndFocus() + ' (throttled)';
      case LifecycleUnitState.FROZEN:
        return 'frozen';
      case LifecycleUnitState.DISCARDED:
        return 'discarded (' + this.discardReasonToString_(reason) + ')' +
            ((reason === LifecycleUnitDiscardReason.URGENT) ? ' at ' +
                     // Must convert since Date constructor takes
                     // milliseconds.
                     (new Date(Number(stateChangeTime.microseconds) / 1000)
                          .toLocaleString()) :
                                                              '');
    }
  }

  /** Dispatches a request to update tabInfos_. */
  private updateTableImpl_() {
    this.discardsDetailsProvider_!.getTabDiscardsInfo().then(response => {
      this.tabInfos_ = response.infos;
    });
  }

  /**
   * A wrapper to updateTableImpl_ that is called due to user action and not
   * due to the automatic timer. Cancels the existing timer  and reschedules
   * it after rendering instantaneously.
   */
  private updateTable_() {
    if (this.updateTimer_) {
      clearInterval(this.updateTimer_);
    }
    this.updateTableImpl_();
    this.updateTimer_ = setInterval(this.updateTableImpl_.bind(this), 1000);
  }

  /**
   * Formats an items site engagement score for display.
   * @param item The item in question.
   * @return The formatted site engagemetn score.
   */
  private getSiteEngagementScore_(item: TabDiscardsInfo): string {
    return item.siteEngagementScore.toFixed(1);
  }

  /**
   * Retrieves favicon style tag value for an item.
   * @param item The item in question.
   * @return A style to retrieve and display the item's favicon.
   */
  private getFavIconStyle_(item: TabDiscardsInfo): string {
    return 'background-image:' + getFaviconForPageURL(item.tabUrl, false);
  }

  /**
   * Formats an items lifecycle state for display.
   * @param item The item in question.
   * @return A human readable lifecycle state.
   */
  private getLifeCycleState_(item: TabDiscardsInfo): string {
    if (item.loadingState !== LifecycleUnitLoadingState.UNLOADED ||
        item.discardCount > 0) {
      return this.lifecycleStateToString_(
          item.state, item.discardReason, item.visibility, item.hasFocus,
          item.stateChangeTime);
    } else {
      return '';
    }
  }

  /**
   * Returns a string representation of a boolean value for display in a
   * table.
   * @param value A boolean value.
   * @return A string representing the bool.
   */
  private boolToString_(value: boolean): string {
    return boolToString(value);
  }

  /**
   * Converts a |secondsAgo| duration to a user friendly string.
   * @param secondsAgo The duration to render.
   * @return An English string representing the duration.
   */
  private durationToString_(secondsAgo: number): string {
    return durationToString(secondsAgo);
  }

  /**
   * Tests whether an item has reasons why it cannot be discarded.
   * @param item The item in question.
   * @return true iff there are reasons why the item cannot be discarded.
   */
  private hasCannotDiscardReasons_(item: TabDiscardsInfo): boolean {
    return item.cannotDiscardReasons.length !== 0;
  }

  /**
   * Tests whether an item can be loaded.
   * @param item The item in question.
   * @return true iff the item can be loaded.
   */
  private canLoad_(item: TabDiscardsInfo): boolean {
    return item.loadingState === LifecycleUnitLoadingState.UNLOADED;
  }

  /**
   * Tests whether an item can be discarded.
   * @param item The item in question.
   * @return true iff the item can be discarded.
   */
  private canDiscard_(item: TabDiscardsInfo): boolean {
    if (item.visibility === LifecycleUnitVisibility.HIDDEN ||
        item.visibility === LifecycleUnitVisibility.OCCLUDED) {
      // Only tabs that aren't visible can be discarded for now.
      switch (item.state) {
        case LifecycleUnitState.DISCARDED:
          return false;
      }
      return true;
    }
    return false;
  }

  /**
   * Tests whether an item should show the reason why it cannot be discarded.
   * @param item The item in question.
   * @return true iff the item should show the reason why it cannot be
   *     discarded.
   */
  private shouldShowCannotDiscardReason_(item: TabDiscardsInfo): boolean {
    return !item.canDiscard && item.state !== LifecycleUnitState.DISCARDED;
  }

  /**
   * Event handler that toggles the auto discardable flag on an item.
   * @param e The event.
   */
  private toggleAutoDiscardable_(e: DomRepeatEvent<TabDiscardsInfo>) {
    const item = e.model.item;
    this.discardsDetailsProvider_!
        .setAutoDiscardable(item.id, !item.isAutoDiscardable)
        .then(this.updateTable_.bind(this));
  }

  /** Event handler that loads a tab. */
  private loadTab_(e: DomRepeatEvent<TabDiscardsInfo>) {
    this.discardsDetailsProvider_!.loadById(e.model.item.id);
  }

  /** Event handler that discards a given tab urgently. */
  private urgentDiscardTab_(e: DomRepeatEvent<TabDiscardsInfo>) {
    this.discardsDetailsProvider_!
        .discardById(e.model.item.id, LifecycleUnitDiscardReason.URGENT)
        .then(this.updateTable_.bind(this));
  }

  /** Event handler that discards a given tab proactively. */
  private proactiveDiscardTab_(e: DomRepeatEvent<TabDiscardsInfo>) {
    this.discardsDetailsProvider_!
        .discardById(e.model.item.id, LifecycleUnitDiscardReason.PROACTIVE)
        .then(this.updateTable_.bind(this));
  }

  /** Implementation function to discard the next discardable tab. */
  private discardImpl_() {
    this.discardsDetailsProvider_!.discard().then(() => {
      this.updateTable_();
    });
  }

  /** Event handler that discards the next discardable tab urgently. */
  private discardUrgentNow_(_e: Event) {
    this.discardImpl_();
  }

  private toggleBatterySaverMode_(_e: Event) {
    this.discardsDetailsProvider_!.toggleBatterySaverMode();
  }

  private refreshPerformanceTabCpuMeasurements_(_e: Event) {
    this.discardsDetailsProvider_!.refreshPerformanceTabCpuMeasurements();
  }
}

customElements.define(DiscardsTabElement.is, DiscardsTabElement);
