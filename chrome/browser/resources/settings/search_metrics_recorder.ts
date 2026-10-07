// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Records UMA metrics about Settings search sessions.
 *
 * Settings elements only report low level facts, either by firing events
 * ('search-finished', 'search-result-interaction') or by updating the 'search'
 * URL query parameter. All the logic that turns those facts into metrics lives
 * here, so that the elements stay free of metrics bookkeeping and so that the
 * mapping can be unit tested in isolation.
 *
 * A search session starts when a non-empty 'search' query parameter appears in
 * the URL, and ends when the user interacts with the results (opens one or
 * changes its value), navigates away using the sidebar, clears the query, or
 * leaves the page. Only the first interaction is observed. For example,
 * opening a result that leads to a subpage ends the session, and what the user
 * does on that subpage is not attributed to the search.
 */

import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import type {MetricsBrowserProxy} from './metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SettingsSearchExitReasonDesktop} from './metrics_browser_proxy.js';
import {routes} from './route.js';
import type {Route, RouteObserverMixinInterface} from './router.js';
import {Router} from './router.js';

/**
 * How long the 'search' URL parameter needs to stay unchanged before the query
 * it carries counts as submitted. The parameter is updated as the user types,
 * so a single query can arrive here as a sequence of prefixes. Metrics only
 * care about the query the user settled on, so those prefixes are collapsed
 * into one submitted query.
 */
export const QUERY_SUBMITTED_DELAY_MS: number = 1000;

/**
 * Dwell times longer than this are assumed to belong to an abandoned session
 * (for example a Settings tab left open in the background), and are dropped.
 */
export const MAX_DWELL_TIME_MS: number = 60 * 60 * 1000;

interface SearchResultInfo {
  query: string;
  matchCount: number;
}

export class SearchMetricsRecorder implements RouteObserverMixinInterface {
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  /** Last 'search' query parameter seen in the URL. */
  private currentQuery_: string = '';

  /** Whether a search session is currently being tracked. */
  private sessionActive_: boolean = false;

  private querySubmittedTimerId_: number|null = null;

  /** Query whose Settings.Search.HasResults sample is still pending. */
  private pendingHasResultsQuery_: string|null = null;

  /** Results of the most recently completed search request. */
  private lastResult_: SearchResultInfo|null = null;

  /** Time at which the user last interacted with a search result. */
  private resultInteractionTime_: number|null = null;

  /** Whether the recorder is currently observing search activity. */
  private running_: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();

  start() {
    if (this.running_) {
      return;
    }
    this.running_ = true;

    Router.getInstance().addObserver(this);

    // `pagehide` fires when navigating away or closing the tab, which is when
    // an ongoing search session gets abandoned. `unload` and `beforeunload`
    // are avoided since they are unreliable, and `visibilitychange` would also
    // fire when merely switching tabs, which does not end a search session.
    // Many search sessions can come and go while the recorder is running, so
    // instead of adding and removing this listener for each one of them, it is
    // registered once here. `endSession_()` does nothing unless a session is
    // in progress.
    this.eventTracker_.add(
        window, 'pagehide',
        () => this.endSession_(SettingsSearchExitReasonDesktop.EXIT_SETTINGS));

    // Pick up searches that are already part of the URL, for example when
    // chrome://settings/?search=foo is opened directly.
    this.currentRouteChanged(Router.getInstance().getCurrentRoute());
  }

  stop() {
    if (!this.running_) {
      return;
    }
    this.running_ = false;

    Router.getInstance().removeObserver(this);
    this.eventTracker_.removeAll();
    this.clearQuerySubmittedTimer_();
    this.endSessionState_();

    this.currentQuery_ = '';
    this.lastResult_ = null;
    this.resultInteractionTime_ = null;
  }

  /**
   * Called when a search request initiated by `settings-main` completed.
   */
  onSearchFinished(query: string, matchCount: number) {
    this.lastResult_ = {query, matchCount};
    this.maybeRecordHasResults_();
  }

  /**
   * Called when the user interacted with one of the search results, either by
   * opening it or by changing its value in place.
   */
  onSearchResultInteraction() {
    if (!this.sessionActive_) {
      return;
    }

    this.resultInteractionTime_ = Date.now();
    this.endSession_(SettingsSearchExitReasonDesktop.INTERACTED_WITH_RESULT);
  }

  currentRouteChanged(newRoute: Route) {
    const query = Router.getInstance().getQueryParameters().get('search') || '';
    if (query === this.currentQuery_) {
      return;
    }

    this.currentQuery_ = query;

    if (query !== '') {
      this.startOrUpdateSession_();
      return;
    }

    // The query was removed from the URL. Back/forward navigations are not
    // attributable to a specific user action on the page, so they get their
    // own bucket. Otherwise, clearing the search box navigates back to the top
    // level route, whereas the sidebar navigates to a section.
    let reason: SettingsSearchExitReasonDesktop;
    if (Router.getInstance().lastRouteChangeWasPopstate()) {
      reason = SettingsSearchExitReasonDesktop.BACK_NAVIGATION;
    } else if (newRoute === routes.BASIC) {
      reason = SettingsSearchExitReasonDesktop.CLEAR_QUERY;
    } else {
      reason = SettingsSearchExitReasonDesktop.NAVIGATE_SIDEBAR;
    }
    this.endSession_(reason);
  }

  private startOrUpdateSession_() {
    this.sessionActive_ = true;

    this.recordDwellTime_();

    // Restart the debounce timer, so that only the query the user settled on
    // is counted as submitted.
    this.clearQuerySubmittedTimer_();
    this.querySubmittedTimerId_ = setTimeout(() => {
      this.querySubmittedTimerId_ = null;
      this.recordQuerySubmitted_();
    }, QUERY_SUBMITTED_DELAY_MS);
  }

  private endSession_(reason: SettingsSearchExitReasonDesktop) {
    if (!this.sessionActive_) {
      return;
    }

    if (this.querySubmittedTimerId_ !== null) {
      this.clearQuerySubmittedTimer_();

      if (reason === SettingsSearchExitReasonDesktop.CLEAR_QUERY ||
          reason === SettingsSearchExitReasonDesktop.EXIT_SETTINGS ||
          reason === SettingsSearchExitReasonDesktop.BACK_NAVIGATION) {
        // The user never settled on a query. Drop the session to avoid
        // skewing the funnel or recording async metrics during page teardown.
        this.endSessionState_();
        return;
      }

      // The user interacted with the results before the debounce fired, which
      // is a stronger signal than the timer, so count the query as submitted.
      this.recordQuerySubmitted_();
    }

    this.metricsBrowserProxy_.recordSettingsSearchExitReason(reason);
    this.endSessionState_();
  }

  private endSessionState_() {
    this.sessionActive_ = false;
    this.pendingHasResultsQuery_ = null;
  }

  private recordQuerySubmitted_() {
    this.metricsBrowserProxy_.recordAction('Settings.Search.QuerySubmitted');
    this.metricsBrowserProxy_.recordSettingsSearchQueryEntered();

    // The results for this query may not be known yet, in which case they are
    // recorded once the search request completes.
    this.pendingHasResultsQuery_ = this.currentQuery_;
    this.maybeRecordHasResults_();
  }

  private maybeRecordHasResults_() {
    if (this.pendingHasResultsQuery_ === null || this.lastResult_ === null ||
        this.lastResult_.query !== this.pendingHasResultsQuery_) {
      return;
    }

    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.Search.HasResults', this.lastResult_.matchCount > 0);
    this.pendingHasResultsQuery_ = null;
  }

  /**
   * Records how long the user spent on a search result before coming back to
   * the search box, if applicable.
   */
  private recordDwellTime_() {
    if (this.resultInteractionTime_ === null) {
      return;
    }

    const dwellTime = Date.now() - this.resultInteractionTime_;
    this.resultInteractionTime_ = null;

    if (dwellTime < MAX_DWELL_TIME_MS) {
      this.metricsBrowserProxy_.recordSettingsSearchResultDwellTime(dwellTime);
    }
  }

  private clearQuerySubmittedTimer_() {
    if (this.querySubmittedTimerId_ !== null) {
      clearTimeout(this.querySubmittedTimerId_);
      this.querySubmittedTimerId_ = null;
    }
  }
}
