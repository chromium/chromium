// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * NuxNtpBackgroundInteractions enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const NuxNtpBackgroundInteractions = {
  PageShown: 0,
  DidNothingAndNavigatedAway: 1,
  DidNothingAndChoseSkip: 2,
  DidNothingAndChoseNext: 3,
  ChoseAnOptionAndNavigatedAway: 4,
  ChoseAnOptionAndChoseSkip: 5,
  ChoseAnOptionAndChoseNext: 6,
  NavigatedAwayThroughBrowserHistory: 7,
  BackgroundImageFailedToLoad: 8,
  BackgroundImageNeverLoaded: 9,
};

/**
 * NuxGoogleAppsInteractions enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const NuxGoogleAppsInteractions = {
  PageShown: 0,
  NotUsed_DEPRECATED: 1,
  GetStarted_DEPRECATED: 2,
  DidNothingAndNavigatedAway: 3,
  DidNothingAndChoseSkip: 4,
  ChoseAnOptionAndNavigatedAway: 5,
  ChoseAnOptionAndChoseSkip: 6,
  ChoseAnOptionAndChoseNext: 7,
  ClickedDisabledNextButtonAndNavigatedAway: 8,
  ClickedDisabledNextButtonAndChoseSkip: 9,
  ClickedDisabledNextButtonAndChoseNext: 10,
  DidNothingAndChoseNext: 11,
  NavigatedAwayThroughBrowserHistory: 12,
};

/** @interface */
class ModuleMetricsProxy {
  recordPageShown() {}

  recordDidNothingAndNavigatedAway() {}

  recordDidNothingAndChoseSkip() {}

  recordDidNothingAndChoseNext() {}

  recordChoseAnOptionAndNavigatedAway() {}

  recordChoseAnOptionAndChoseSkip() {}

  recordChoseAnOptionAndChoseNext() {}

  recordClickedDisabledNextButtonAndNavigatedAway() {}

  recordClickedDisabledNextButtonAndChoseSkip() {}

  recordClickedDisabledNextButtonAndChoseNext() {}

  recordNavigatedAwayThroughBrowserHistory() {}
}

/** @implements {ModuleMetricsProxy} */
export class ModuleMetricsProxyImpl {
  /**
   * @param {string} histogramName The histogram that will record the module
   *      navigation metrics.
   */
  constructor(histogramName, interactions) {
    /** @private {string} */
    this.interactionMetric_ = histogramName;
    this.interactions_ = interactions;
  }

  /** @override */
  recordPageShown() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.PageShown,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordDidNothingAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordDidNothingAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordDidNothingAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordChoseAnOptionAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ChoseAnOptionAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordChoseAnOptionAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.ChoseAnOptionAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordChoseAnOptionAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.ChoseAnOptionAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordClickedDisabledNextButtonAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordClickedDisabledNextButtonAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.NavigatedAwayThroughBrowserHistory,
        Object.keys(this.interactions_).length);
  }
}

export class ModuleMetricsManager {
  /** @param {ModuleMetricsProxy} metricsProxy */
  constructor(metricsProxy) {
    this.metricsProxy_ = metricsProxy;

    this.options_ = {
      didNothing: {
        andNavigatedAway: metricsProxy.recordDidNothingAndNavigatedAway,
        andChoseSkip: metricsProxy.recordDidNothingAndChoseSkip,
        andChoseNext: metricsProxy.recordDidNothingAndChoseNext,
      },
      choseAnOption: {
        andNavigatedAway: metricsProxy.recordChoseAnOptionAndNavigatedAway,
        andChoseSkip: metricsProxy.recordChoseAnOptionAndChoseSkip,
        andChoseNext: metricsProxy.recordChoseAnOptionAndChoseNext,
      },
      clickedDisabledNextButton: {
        andNavigatedAway:
            metricsProxy.recordClickedDisabledNextButtonAndNavigatedAway,
        andChoseSkip: metricsProxy.recordClickedDisabledNextButtonAndChoseSkip,
        andChoseNext: metricsProxy.recordClickedDisabledNextButtonAndChoseNext,
      },
    };

    this.firstPart = this.options_.didNothing;
  }

  recordPageInitialized() {
    this.metricsProxy_.recordPageShown();
    this.firstPart = this.options_.didNothing;
  }

  recordClickedOption() {
    // Only overwrite this.firstPart if it's not overwritten already
    if (this.firstPart == this.options_.didNothing) {
      this.firstPart = this.options_.choseAnOption;
    }
  }

  recordClickedDisabledButton() {
    // Only overwrite this.firstPart if it's not overwritten already
    if (this.firstPart == this.options_.didNothing) {
      this.firstPart = this.options_.clickedDisabledNextButton;
    }
  }

  recordNoThanks() {
    this.firstPart.andChoseSkip.call(this.metricsProxy_);
  }

  recordGetStarted() {
    this.firstPart.andChoseNext.call(this.metricsProxy_);
  }

  recordNavigatedAway() {
    this.firstPart.andNavigatedAway.call(this.metricsProxy_);
  }

  recordBrowserBackOrForward() {
    this.metricsProxy_.recordNavigatedAwayThroughBrowserHistory();
  }
}
