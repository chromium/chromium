// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import type {LatencyJourney, OmniboxInputState} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup.mojom-webui.js';

// A journey that waits for the document to become visible, with what the
// recorder needs to know of the state that it ends on.
interface JourneyAwaitingVisibility {
  journey: LatencyJourney;
  tabId: number;
  userInputInProgress: boolean;
}

/**
 * Records the latency journeys that the browser sends with input states
 * (`OmniboxInputState.latencyJourney`). Each journey ends with the presentation
 * time of the first frame after its state is applied, and is reported to the
 * browser with `MetricsReporter.umaReportLongTimeSpan()`.
 *
 * Its owner tells it when a state is applied, when the browser replaces the
 * input's state or focus, and when the document becomes visible.
 */
export class LatencyJourneyRecorder {
  // The journey of a state applied while the document was hidden.
  // `chrome.requestPresentationTime()` can't observe a hidden document's
  // frames, so it ends with the first frame after the document becomes
  // visible, unless a later state replaces its state first. Only one journey
  // waits at a time, since a state with a journey of its own replaces the
  // state that the waiting journey ends on.
  private journeyAwaitingVisibility_: JourneyAwaitingVisibility|null = null;

  /**
   * `waitForRender_` resolves once the owner's pending DOM updates (e.g. Lit
   * renders) are done, still in the same task, so that the next frame shows
   * them.
   */
  constructor(private readonly waitForRender_: () => Promise<unknown>) {}

  /**
   * Called after the owner applies `state`, in the same task, so that the next
   * frame shows it. Drops the waiting journey if `state` replaces the state
   * that it ends on (see `shouldDropWaitingJourney_()`). Then records the
   * journey of `state`, if any: with the next frame if the document is
   * visible, or else with the first frame after it becomes visible.
   */
  onInputStateApplied(state: OmniboxInputState) {
    if (this.shouldDropWaitingJourney_(state)) {
      this.onInputStateReplaced();
    }
    if (state.latencyJourney === null) {
      return;
    }
    if (document.visibilityState === 'visible') {
      this.reportOnNextPresentedFrame_(state.latencyJourney);
    } else {
      this.journeyAwaitingVisibility_ = {
        journey: state.latencyJourney,
        tabId: state.tabId,
        userInputInProgress: state.userInputInProgress,
      };
    }
  }

  /**
   * Called when the browser blurs the input, enters keyword mode or clears the
   * popup. Drops the journey waiting for the document to become visible, since
   * the first frame after that no longer shows the state that it ends on.
   * Journeys reported while the document is visible still end with their next
   * frame.
   */
  onInputStateReplaced() {
    this.journeyAwaitingVisibility_ = null;
  }

  /**
   * Called when the document becomes visible, after the owner applies any focus
   * that it deferred while hidden. Reports the waiting journey with the first
   * frame of the show.
   */
  onDocumentVisible() {
    const awaiting = this.journeyAwaitingVisibility_;
    if (awaiting === null) {
      return;
    }
    this.journeyAwaitingVisibility_ = null;
    this.reportOnNextPresentedFrame_(awaiting.journey);
  }

  /**
   * Whether applying `state` drops the waiting journey: it does if `state`
   * replaces the state that the journey ends on, i.e. if it has a journey of
   * its own, blurs the input, shows another tab or starts user input. Other
   * states, e.g. a focus restored right after a tab change, only refine it.
   */
  private shouldDropWaitingJourney_(state: OmniboxInputState): boolean {
    const awaiting = this.journeyAwaitingVisibility_;
    if (awaiting === null) {
      return false;
    }
    // `isTabSwitch` is false for the first state of a tab without a saved
    // omnibox state that takes focus, e.g. a new NTP (see
    // `OmniboxPopupViewFullWebUI::OnTabChanged()`), so also compare the tab
    // IDs, unless the waiting journey's tab is unknown (ID 0).
    const showsAnotherTab = state.isTabSwitch ||
        (awaiting.tabId !== 0 && state.tabId !== awaiting.tabId);
    const startsUserInput =
        state.userInputInProgress && !awaiting.userInputInProgress;
    return state.latencyJourney !== null || !state.isFocused ||
        showsAnotherTab || startsUserInput;
  }

  /**
   * Reports `journey` to the browser, which records it in its UMA histogram,
   * with the presentation time of the next frame: the first one that shows the
   * DOM changes made in this task, including the owner's pending renders. Must
   * be called in that task, while the document is visible:
   * `chrome.requestPresentationTime()` observes the next frame without
   * requesting one.
   */
  private async reportOnNextPresentedFrame_(journey: LatencyJourney) {
    // Lit renders property changes in a microtask, and
    // `chrome.requestPresentationTime()` only observes a frame that the DOM
    // changes made so far have requested. Waiting for the renders keeps the
    // request in this task, before that frame.
    await this.waitForRender_();
    const time = await chrome.requestPresentationTime();
    if (time === null) {
      // The frame wasn't presented, or this task changed nothing. Marks in
      // traces why the journey got no time.
      // TODO(b/578627841): Also record dropped journeys in UMA, e.g. with a
      // per-journey outcome, including the journeys dropped while waiting for
      // the document to become visible, so that field data shows whether drops
      // skew the histograms.
      performance.mark(
          'LatencyJourneyRecorder::reportOnNextPresentedFrame_:NotPresented');
      return;
    }
    MetricsReporterImpl.getInstance().umaReportLongTimeSpan(
        journey.histogramName, journey.startTime.internalValue, time);
  }
}
