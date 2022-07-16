// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * NuxNtpBackgroundInteractions enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum NuxNtpBackgroundInteractions {
  PageShown = 0,
  DidNothingAndNavigatedAway,
  DidNothingAndChoseSkip,
  DidNothingAndChoseNext,
  ChoseAnOptionAndNavigatedAway,
  ChoseAnOptionAndChoseSkip,
  ChoseAnOptionAndChoseNext,
  NavigatedAwayThroughBrowserHistory,
  BackgroundImageFailedToLoad,
  BackgroundImageNeverLoaded,
}

/**
 * NuxGoogleAppsInteractions enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 */
export enum NuxGoogleAppsInteractions {
  PageShown = 0,
  NotUsed_DEPRECATED,
  GetStarted_DEPRECATED,
  DidNothingAndNavigatedAway,
  DidNothingAndChoseSkip,
  ChoseAnOptionAndNavigatedAway,
  ChoseAnOptionAndChoseSkip,
  ChoseAnOptionAndChoseNext,
  ClickedDisabledNextButtonAndNavigatedAway,
  ClickedDisabledNextButtonAndChoseSkip,
  ClickedDisabledNextButtonAndChoseNext,
  DidNothingAndChoseNext,
  NavigatedAwayThroughBrowserHistory,
}

export interface ModuleMetricsProxy {
  recordPageShown(): void;
  recordDidNothingAndNavigatedAway(): void;
  recordDidNothingAndChoseSkip(): void;
  recordDidNothingAndChoseNext(): void;
  recordChoseAnOptionAndNavigatedAway(): void;
  recordChoseAnOptionAndChoseSkip(): void;
  recordChoseAnOptionAndChoseNext(): void;
  recordClickedDisabledNextButtonAndNavigatedAway(): void;
  recordClickedDisabledNextButtonAndChoseSkip(): void;
  recordClickedDisabledNextButtonAndChoseNext(): void;
  recordNavigatedAwayThroughBrowserHistory(): void;
}

export class ModuleMetricsProxyImpl implements ModuleMetricsProxy {
  private interactionMetric_: string;
  private interactions_: any;

  /**
   * @param histogramName The histogram that will record the module
   *      navigation metrics.
   */
  constructor(histogramName: string, interactions: any) {
    this.interactionMetric_ = histogramName;
    this.interactions_ = interactions;
  }

  recordPageShown() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.PageShown,
        Object.keys(this.interactions_).length);
  }

  recordDidNothingAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  recordDidNothingAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  recordDidNothingAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.DidNothingAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  recordChoseAnOptionAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ChoseAnOptionAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  recordChoseAnOptionAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.ChoseAnOptionAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  recordChoseAnOptionAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_, this.interactions_.ChoseAnOptionAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  recordClickedDisabledNextButtonAndNavigatedAway() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndNavigatedAway,
        Object.keys(this.interactions_).length);
  }

  recordClickedDisabledNextButtonAndChoseSkip() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndChoseSkip,
        Object.keys(this.interactions_).length);
  }

  recordClickedDisabledNextButtonAndChoseNext() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.ClickedDisabledNextButtonAndChoseNext,
        Object.keys(this.interactions_).length);
  }

  recordNavigatedAwayThroughBrowserHistory() {
    chrome.metricsPrivate.recordEnumerationValue(
        this.interactionMetric_,
        this.interactions_.NavigatedAwayThroughBrowserHistory,
        Object.keys(this.interactions_).length);
  }
}

export class ModuleMetricsManager {
  private metricsProxy_: ModuleMetricsProxy;
  private options_: any;
  firstPart: any;

  constructor(metricsProxy: ModuleMetricsProxy) {
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
    if (this.firstPart === this.options_.didNothing) {
      this.firstPart = this.options_.choseAnOption;
    }
  }

  recordClickedDisabledButton() {
    // Only overwrite this.firstPart if it's not overwritten already
    if (this.firstPart === this.options_.didNothing) {
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
