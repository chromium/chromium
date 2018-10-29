// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {

  // The metrics name corresponding to Nux EmailProvidersInteraction histogram.
  const INTERACTION_METRIC_NAME =
      'FirstRun.NewUserExperience.EmailProvidersInteraction';

  const SELECTION_METRIC_NAME =
      'FirstRun.NewUserExperience.EmailProvidersSelection';

  /**
   * NuxEmailProvidersInteractions enum.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxEmailProvidersInteractions = {
    PageShown: 0,
    DidNothingAndNavigatedAway: 1,
    DidNothingAndChoseSkip: 2,
    ChoseAnOptionAndNavigatedAway: 3,
    ChoseAnOptionAndChoseSkip: 4,
    ChoseAnOptionAndChoseNext: 5,
    ClickedDisabledNextButtonAndNavigatedAway: 6,
    ClickedDisabledNextButtonAndChoseSkip: 7,
    ClickedDisabledNextButtonAndChoseNext: 8,
  };

  /**
   * The number of enum values in NuxEmailProvidersInteractions. This should
   * be kept in sync with the enum count in tools/metrics/histograms/enums.xml.
   * @type {number}
   */
  const INTERACTION_METRIC_COUNT =
      Object.keys(NuxEmailProvidersInteractions).length;

  /** @interface */
  class NuxEmailProxy {
    /**
     * Email provider IDs are local to the list of email providers, so their
     * icon must be cached by the handler that provided the IDs.
     * @param {number} emailProviderId
     */
    cacheBookmarkIcon(emailProviderId) {}

    /**
     * Returns a promise for an array of email providers.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getEmailList() {}

    /** @return {number} */
    getSavedProvider() {}

    recordPageInitialized() {}

    recordClickedOption() {}

    recordClickedDisabledButton() {}

    /**
     * @param {number} providerId This should match one of the histogram enum
     *     value for NuxEmailProvidersSelections.
     * @param {number} length
     */
    recordProviderSelected(providerId, length) {}

    recordNoThanks() {}

    recordGetStarted() {}

    recordFinalize() {}
  }

  /** @implements {nux.NuxEmailProxy} */
  class NuxEmailProxyImpl {
    constructor() {
      /** @private {string} */
      this.firstPart = '';

      /** @private {string} */
      this.lastPart = '';

      /** @private {number} */
      this.savedProvider_;
    }

    /** @override */
    cacheBookmarkIcon(emailProviderId) {
      chrome.send('cacheEmailIcon', [emailProviderId]);
    }

    /** @override */
    getEmailList() {
      return cr.sendWithPromise('getEmailList');
    }

    /** @override */
    getSavedProvider() {
      return this.savedProvider_;
    }

    /** @override */
    recordPageInitialized() {
      chrome.metricsPrivate.recordEnumerationValue(
          INTERACTION_METRIC_NAME, NuxEmailProvidersInteractions.PageShown,
          INTERACTION_METRIC_COUNT);

      // These two flags are used at the end to determine what to record in
      // metrics. Their values should map to first or last half of an enum
      // name within NuxEmailProvidersInteractions.
      this.firstPart = 'DidNothing';
      this.lastPart = 'AndNavigatedAway';
    }

    /** @override */
    recordClickedOption() {
      // Only overwrite this.firstPart if it's not overwritten already
      if (this.firstPart == 'DidNothing')
        this.firstPart = 'ChoseAnOption';
    }

    /** @override */
    recordClickedDisabledButton() {
      // Only overwrite this.firstPart if it's not overwritten already
      if (this.firstPart == 'DidNothing')
        this.firstPart = 'ClickedDisabledNextButton';
    }

    /** @override */
    recordProviderSelected(providerId, length) {
      this.savedProvider_ = providerId;
      // TODO(hcarmona): get enum's max from loadTimeData instead, since length
      //     might not be accurate once we start localizing.
      chrome.metricsPrivate.recordEnumerationValue(
          SELECTION_METRIC_NAME, providerId, length);
    }

    /** @override */
    recordNoThanks() {
      this.lastPart = 'AndChoseSkip';
      this.recordFinalize();
    }

    /** @override */
    recordGetStarted() {
      this.lastPart = 'AndChoseNext';
      this.recordFinalize();
    }

    /** @override */
    recordFinalize() {
      let finalValue = this.firstPart + this.lastPart;

      chrome.metricsPrivate.recordEnumerationValue(
          INTERACTION_METRIC_NAME, NuxEmailProvidersInteractions[finalValue],
          INTERACTION_METRIC_COUNT);
    }
  }

  cr.addSingletonGetter(NuxEmailProxyImpl);

  return {
    NuxEmailProxy: NuxEmailProxy,
    NuxEmailProxyImpl: NuxEmailProxyImpl,
  };
});
