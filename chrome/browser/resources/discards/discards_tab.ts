// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/action_link.js';
import '/strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {boolToString, durationToString, getOrCreateDetailsProvider} from './discards.js';
import type {DetailsProviderRemote, TabDiscardsInfo} from './discards.mojom-webui.js';
import {CanFreeze, LifecycleUnitVisibility} from './discards.mojom-webui.js';
import {getCss} from './discards_tab.css.js';
import {getHtml} from './discards_tab.html.js';
import {LifecycleUnitDiscardReason, LifecycleUnitLoadingState, LifecycleUnitState} from './lifecycle_unit_state.mojom-webui.js';
import {SortedTableMixinLit} from './sorted_table_mixin_lit.js';

function compareByTitle(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  const val1 = a.title.toLowerCase();
  const val2 = b.title.toLowerCase();
  if (val1 === val2) {
    return 0;
  }
  return val1 > val2 ? 1 : -1;
}

function compareByTabUrl(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  const val1 = a.tabUrl.toLowerCase();
  const val2 = b.tabUrl.toLowerCase();
  if (val1 === val2) {
    return 0;
  }
  return val1 > val2 ? 1 : -1;
}

function compareByIsAutoDiscardable(
    a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  const val1 = a.isAutoDiscardable;
  const val2 = b.isAutoDiscardable;
  if (val1 === val2) {
    return 0;
  }
  return val1 > val2 ? 1 : -1;
}

function compareByCanDiscard(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  const val1 = a.canDiscard;
  const val2 = b.canDiscard;
  if (val1 === val2) {
    return 0;
  }
  return val1 > val2 ? 1 : -1;
}

function compareByState(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  const val1 = a.state;
  const val2 = b.state;
  // If the keys are discarding state, then break ties using the discard
  // reason.
  if (val1 === val2 && val1 === LifecycleUnitState.DISCARDED) {
    return a.discardReason - b.discardReason;
  }
  return val1 - val2;
}

function compareByVisibility(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return (a.visibility as number) - (b.visibility as number);
}

function compareByLoadingState(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.loadingState - b.loadingState;
}

function compareByDiscardCount(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.discardCount - b.discardCount;
}

function compareByUtilityRank(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.utilityRank - b.utilityRank;
}

function compareByLastActiveSeconds(
    a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.lastActiveSeconds - b.lastActiveSeconds;
}

function compareBySiteEngagementScore(
    a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.siteEngagementScore - b.siteEngagementScore;
}

function compareByCanFreeze(a: TabDiscardsInfo, b: TabDiscardsInfo): number {
  return a.canFreeze - b.canFreeze;
}

/**
 * @param sortKey The sort key to get a function for.
 * @return
 *     A comparison function that compares two site data entries, returns
 *     negative number if a < b, 0 if a === b, and a positive
 *     number if a > b.
 */
export function getSortFunctionForKey(sortKey: string): (
    a: TabDiscardsInfo, b: TabDiscardsInfo) => number {
  switch (sortKey) {
    case 'title':
      return compareByTitle;
    case 'tabUrl':
      return compareByTabUrl;
    case 'isAutoDiscardable':
      return compareByIsAutoDiscardable;
    case 'canDiscard':
      return compareByCanDiscard;
    case 'state':
      return compareByState;
    case 'visibility':
      return compareByVisibility;
    case 'loadingState':
      return compareByLoadingState;
    case 'discardCount':
      return compareByDiscardCount;
    case 'utilityRank':
      return compareByUtilityRank;
    case 'lastActiveSeconds':
      return compareByLastActiveSeconds;
    case 'siteEngagementScore':
      return compareBySiteEngagementScore;
    case 'canFreeze':
      return compareByCanFreeze;
    default:
      assertNotReached('Unknown sortKey: ' + sortKey);
  }
}

const DiscardsTabElementBase = SortedTableMixinLit(CrLitElement);

export class DiscardsTabElement extends DiscardsTabElementBase {
  static get is() {
    return 'discards-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabInfos_: {type: Array},
      isPerformanceInterventionDemoModeEnabled_: {type: Boolean},
    };
  }

  protected accessor tabInfos_: TabDiscardsInfo[] = [];
  protected accessor isPerformanceInterventionDemoModeEnabled_: boolean =
      loadTimeData.getBoolean('isPerformanceInterventionDemoModeEnabled');

  /** The current update timer if any. */
  private updateTimer_: number = 0;

  private discardsDetailsProvider_: DetailsProviderRemote|null = null;

  override sortKey: string = 'utilityRank';

  override connectedCallback() {
    super.connectedCallback();

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
      (a: TabDiscardsInfo, b: TabDiscardsInfo) => number {
    const sortFunction = getSortFunctionForKey(sortKey);
    return function(a: TabDiscardsInfo, b: TabDiscardsInfo) {
      const comp = sortFunction(a, b);
      return sortReverse ? -comp : comp;
    };
  }

  protected getSortedTabInfos_(): TabDiscardsInfo[] {
    if (!this.tabInfos_) {
      return [];
    }
    const sortFunction =
        this.computeSortFunction_(this.sortKey, this.sortReverse);
    return this.tabInfos_.sort(sortFunction);
  }

  /**
   * Returns a string representation of a visibility enum value for display in
   * a table.
   * @param visibility A visibility value.
   * @return A string representation of the visibility.
   */
  protected visibilityToString_(visibility: LifecycleUnitVisibility): string {
    switch (visibility) {
      case LifecycleUnitVisibility.HIDDEN:
        return 'hidden';
      case LifecycleUnitVisibility.OCCLUDED:
        return 'occluded';
      case LifecycleUnitVisibility.VISIBLE:
        return 'visible';
      default:
        assertNotReached();
    }
  }

  /**
   * Returns a string representation of a loading state enum value for display
   * in a table.
   * @param loadingState A loading state value.
   * @return A string representation of the loading state.
   */
  protected loadingStateToString_(loadingState: LifecycleUnitLoadingState):
      string {
    switch (loadingState) {
      case LifecycleUnitLoadingState.UNLOADED:
        return 'unloaded';
      case LifecycleUnitLoadingState.LOADING:
        return 'loading';
      case LifecycleUnitLoadingState.LOADED:
        return 'loaded';
      default:
        assertNotReached();
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
      case LifecycleUnitDiscardReason.FROZEN_WITH_GROWING_MEMORY:
        return 'frozen with growing memory';
      default:
        assertNotReached();
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
        default:
          assertNotReached();
      }
    }

    switch (state) {
      case LifecycleUnitState.ACTIVE:
        return pageLifecycleStateFromVisibilityAndFocus();
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
      default:
        assertNotReached();
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
  protected getSiteEngagementScore_(item: TabDiscardsInfo): string {
    return item.siteEngagementScore.toFixed(1);
  }

  /**
   * Retrieves favicon style tag value for an item.
   * @param item The item in question.
   * @return A style to retrieve and display the item's favicon.
   */
  protected getFavIconStyle_(item: TabDiscardsInfo): string {
    return 'background-image:' + getFaviconForPageURL(item.tabUrl, false);
  }

  /**
   * Formats an items lifecycle state for display.
   * @param item The item in question.
   * @return A human readable lifecycle state.
   */
  protected getLifeCycleState_(item: TabDiscardsInfo): string {
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
  protected boolToString_(value: boolean): string {
    return boolToString(value);
  }

  /**
   * Returns a string representation of a CanFreeze value for display in a
   * table.
   * @param value A CanFreeze value.
   * @return A string representing the CanFreeze value.
   */
  protected canFreezeToString_(canFreeze: CanFreeze): string {
    switch (canFreeze) {
      case CanFreeze.YES:
        return '✔';
      case CanFreeze.NO:
        return '✘️';
      case CanFreeze.VARIES:
        return '~';
      default:
        assertNotReached();
    }
  }

  /**
   * Converts a |secondsAgo| duration to a user friendly string.
   * @param secondsAgo The duration to render.
   * @return An English string representing the duration.
   */
  protected durationToString_(secondsAgo: number): string {
    return durationToString(secondsAgo);
  }

  /**
   * Tests whether a tab can be loaded via the discards UI.
   * @param tab The tab.
   * @return true iff the tab can be loaded.
   */
  protected canLoadViaUi_(tab: TabDiscardsInfo): boolean {
    return tab.loadingState === LifecycleUnitLoadingState.UNLOADED;
  }

  /**
   * Tests whether a tab can be discarded via the discards UI. This is different
   * from whether the tab could be automatically be discarded.
   * @param tab The tab.
   * @return true iff the tab can be discarded.
   */
  protected canDiscardViaUi_(tab: TabDiscardsInfo): boolean {
    return tab.visibility !== LifecycleUnitVisibility.VISIBLE &&
        tab.state !== LifecycleUnitState.DISCARDED;
  }

  /**
   * Tests whether a tab can be frozen via the discards UI. This is different
   * from whether the tab could automatically be frozen.
   * @param tab The tab.
   * @return true iff the tab can be frozen.
   */
  // <if expr="not is_android">
  protected canFreezeViaUi_(tab: TabDiscardsInfo): boolean {
    return tab.visibility !== LifecycleUnitVisibility.VISIBLE &&
        tab.state !== LifecycleUnitState.DISCARDED &&
        tab.state !== LifecycleUnitState.FROZEN;
  }
  // </if>
  // <if expr="is_android">
  // TODO(crbug.com/40160563): Add FreezingPolicy to Android.
  protected canFreezeViaUi_(_tab: TabDiscardsInfo): boolean {
    return false;
  }
  // </if>

  /**
   * Tests whether a tab should show the reason why it cannot be discarded.
   * @param tab The tab.
   * @return true iff the tab should show the reason why it cannot be discarded.
   */
  protected shouldShowCannotDiscardReason_(tab: TabDiscardsInfo): boolean {
    return !tab.canDiscard && tab.state !== LifecycleUnitState.DISCARDED;
  }

  /**
   * Tests whether a tab should show the reason why it cannot be frozen.
   * @param tab The tab.
   * @return true iff the tab should show the reason why it cannot be frozen.
   */
  protected shouldShowCannotFreezeReason_(tab: TabDiscardsInfo): boolean {
    return tab.canFreeze !== CanFreeze.YES &&
        tab.state !== LifecycleUnitState.FROZEN &&
        tab.state !== LifecycleUnitState.DISCARDED;
  }

  /**
   * Event handler that toggles the auto discardable flag on an item.
   * @param e The event.
   */
  protected toggleAutoDiscardable_(e: Event) {
    // Uses dataset['id'] and dataset['isAutoDiscardable'] instead of
    // dataset['index'] to avoid the following scenario:
    // 1. The callback in updateTableImpl_() is called to update this.tabInfos_.
    // 2. toggleAutoDiscardable_() is called, then index and this.tabInfos_
    //    would not match.
    // 3. render() is called.
    const item = e.currentTarget as HTMLElement;
    const id = Number(item.dataset['id']);
    const isAutoDiscardable = item.dataset['isAutoDiscardable'] === 'true';
    this.discardsDetailsProvider_!.setAutoDiscardable(id, !isAutoDiscardable)
        .then(this.updateTable_.bind(this));
  }

  /** Event handler that loads a tab. */
  protected loadTab_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    this.discardsDetailsProvider_!.loadById(id);
  }

  /** Event handler that discards a given tab urgently. */
  protected urgentDiscardTab_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    this.discardsDetailsProvider_!
        .discardById(id, LifecycleUnitDiscardReason.URGENT)
        .then(this.updateTable_.bind(this));
  }

  /** Event handler that discards a given tab proactively. */
  protected proactiveDiscardTab_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    this.discardsDetailsProvider_!
        .discardById(id, LifecycleUnitDiscardReason.PROACTIVE)
        .then(this.updateTable_.bind(this));
  }

  /** Event handler that freezes a tab. */
  protected freezeTab_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['id']);
    this.discardsDetailsProvider_!.freezeById(id);
  }

  /** Implementation function to discard the next discardable tab. */
  private discardImpl_() {
    this.discardsDetailsProvider_!.discard().then(() => {
      this.updateTable_();
    });
  }

  /** Event handler that discards the next discardable tab urgently. */
  protected discardUrgentNow_(_e: Event) {
    this.discardImpl_();
  }

  protected toggleBatterySaverMode_(_e: Event) {
    this.discardsDetailsProvider_!.toggleBatterySaverMode();
  }

  protected refreshPerformanceTabCpuMeasurements_(_e: Event) {
    this.discardsDetailsProvider_!.refreshPerformanceTabCpuMeasurements();
  }
}

customElements.define(DiscardsTabElement.is, DiscardsTabElement);
