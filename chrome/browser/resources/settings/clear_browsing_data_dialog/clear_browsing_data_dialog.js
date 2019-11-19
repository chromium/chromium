// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */
Polymer({
  is: 'settings-clear-browsing-data-dialog',

  behaviors: [
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * TODO(dpapad): make |syncStatus| private.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Results of browsing data counters, keyed by the suffix of
     * the corresponding data type deletion preference, as reported
     * by the C++ side.
     * @private {!Object<string>}
     */
    counters_: {
      type: Object,
      // Will be filled as results are reported.
      value: function() {
        return {};
      }
    },

    /**
     * List of options for the dropdown menu.
     * @private {!DropdownMenuOptionList}
     */
    clearFromOptions_: {
      readOnly: true,
      type: Array,
      value: [
        {value: 0, name: loadTimeData.getString('clearPeriodHour')},
        {value: 1, name: loadTimeData.getString('clearPeriod24Hours')},
        {value: 2, name: loadTimeData.getString('clearPeriod7Days')},
        {value: 3, name: loadTimeData.getString('clearPeriod4Weeks')},
        {value: 4, name: loadTimeData.getString('clearPeriodEverything')},
      ],
    },

    /** @private */
    clearingInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    clearButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSupervised_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isSupervised');
      },
    },

    /** @private */
    showHistoryDeletionDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSignedIn_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSyncingHistory_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    shouldShowCookieException_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSyncPaused_: {
      type: Boolean,
      value: false,
      computed: 'computeIsSyncPaused_(syncStatus)',
    },

    /** @private */
    hasPassphraseError_: {
      type: Boolean,
      value: false,
      computed: 'computeHasPassphraseError_(syncStatus)',
    },

    /** @private */
    hasOtherSyncError_: {
      type: Boolean,
      value: false,
      computed:
          'computeHasOtherError_(syncStatus, isSyncPaused_, hasPassphraseError_)',
    },

    /**
     * This flag is used to conditionally show the footer for the dialog.
     * @private
     */
    diceEnabled_: {
      type: Boolean,
      value: function() {
        let diceEnabled = false;
        // <if expr="not chromeos">
        diceEnabled = loadTimeData.getBoolean('diceEnabled');
        // </if>
        return diceEnabled;
      },
    },

    /**
     * Time in ms, when the dialog was opened.
     * @private
     */
    dialogOpenedTime_: {
      type: Number,
      value: 0,
    },

    /** @private {Array<string>} */
    tabsNames_: {
      type: Array,
      value: () =>
          [loadTimeData.getString('basicPageTitle'),
           loadTimeData.getString('advancedPageTitle'),
],
    },
  },

  listeners: {'settings-boolean-control-change': 'updateClearButtonState_'},

  /** @private {settings.ClearBrowsingDataBrowserProxy} */
  browserProxy_: null,

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  ready: function() {
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    this.addWebUIListener(
        'update-sync-state', this.updateSyncState_.bind(this));
    this.addWebUIListener(
        'update-counter-text', this.updateCounterText_.bind(this));
  },

  /** @override */
  attached: function() {
    this.browserProxy_ =
        settings.ClearBrowsingDataBrowserProxyImpl.getInstance();
    this.dialogOpenedTime_ = Date.now();
    this.browserProxy_.initialize().then(() => {
      this.$.clearBrowsingDataDialog.showModal();
    });
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /**
   * Returns true if either clearing is in progress or no data type is selected.
   * @param {boolean} clearingInProgress
   * @param {boolean} clearButtonDisabled
   * @return {boolean}
   * @private
   */
  isClearButtonDisabled_: function(clearingInProgress, clearButtonDisabled) {
    return clearingInProgress || clearButtonDisabled;
  },

  /**
   * Disables the Clear Data button if no data type is selected.
   * @private
   */
  updateClearButtonState_: function() {
    // on-select-item-changed gets called with undefined during a tab change.
    // https://github.com/PolymerElements/iron-selector/issues/95
    const tab = this.$.tabs.selectedItem;
    if (!tab) {
      return;
    }
    this.clearButtonDisabled_ = this.getSelectedDataTypes_(tab).length == 0;
  },

  /**
   * Record visits to the CBD dialog.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    if (currentRoute == settings.routes.CLEAR_BROWSER_DATA) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_DialogCreated');
      this.dialogOpenedTime_ = Date.now();
    }
  },

  /**
   * Updates the history description to show the relevant information
   * depending on sync and signin state.
   *
   * @param {boolean} signedIn Whether the user is signed in.
   * @param {boolean} syncing Whether the user is syncing history.
   * @param {boolean} shouldShowCookieException Whether the exception about not
   *    being signed out of your Google account should be shown.
   * @private
   */
  updateSyncState_: function(signedIn, syncing, shouldShowCookieException) {
    this.isSignedIn_ = signedIn;
    this.isSyncingHistory_ = syncing;
    this.shouldShowCookieException_ = shouldShowCookieException;
    this.$.clearBrowsingDataDialog.classList.add('fully-rendered');
  },

  /**
   * Choose a label for the history checkbox.
   * @param {boolean} isSignedIn
   * @param {boolean} isSyncingHistory
   * @param {string} historySummary
   * @param {string} historySummarySignedIn
   * @param {string} historySummarySynced
   * @return {string}
   * @private
   */
  browsingCheckboxLabel_: function(
      isSignedIn, isSyncingHistory, hasSyncError, historySummary,
      historySummarySignedIn, historySummarySynced) {
    if (isSyncingHistory && !hasSyncError) {
      return historySummarySynced;
    } else if (isSignedIn && !this.isSyncPaused_) {
      return historySummarySignedIn;
    }
    return historySummary;
  },

  /**
   * Choose a label for the cookie checkbox.
   * @param {boolean} shouldShowCookieException
   * @param {string} cookiesSummary
   * @param {string} cookiesSummarySignedIn
   * @return {string}
   * @private
   */
  cookiesCheckboxLabel_: function(
      shouldShowCookieException, cookiesSummary, cookiesSummarySignedIn) {
    if (shouldShowCookieException) {
      return cookiesSummarySignedIn;
    }
    return cookiesSummary;
  },

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param {string} prefName Browsing data type deletion preference.
   * @param {string} text The text with which to update the counter
   * @private
   */
  updateCounterText_: function(prefName, text) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    const matches = prefName.match(/^browser\.clear_data\.(\w+)$/);
    this.set('counters_.' + assert(matches[1]), text);
  },

  /**
   * Returns a list of selected data types.
   * @param {!HTMLElement} tab
   * @return {!Array<string>}
   * @private
   */
  getSelectedDataTypes_: function(tab) {
    const checkboxes = tab.querySelectorAll('settings-checkbox');
    const dataTypes = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.pref.key);
      }
    });
    return dataTypes;
  },

  /**
   * Clears browsing data and maybe shows a history notice.
   * @private
   */
  clearBrowsingData_: function() {
    this.clearingInProgress_ = true;
    const tab = this.$.tabs.selectedItem;
    const dataTypes = this.getSelectedDataTypes_(tab);
    const timePeriod = tab.querySelector('.time-range-select').pref.value;

    if (tab.id == 'basic-tab') {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
    }

    this.browserProxy_.clearBrowsingData(dataTypes, timePeriod)
        .then(shouldShowNotice => {
          this.clearingInProgress_ = false;
          this.showHistoryDeletionDialog_ = shouldShowNotice;
          chrome.metricsPrivate.recordMediumTime(
              'History.ClearBrowsingData.TimeSpentInDialog',
              Date.now() - this.dialogOpenedTime_);
          if (!shouldShowNotice) {
            this.$.clearBrowsingDataDialog.close();
          }
        });
  },

  /** @private */
  onCancelTap_: function() {
    this.$.clearBrowsingDataDialog.cancel();
  },

  /**
   * Handles the closing of the notice about other forms of browsing history.
   * @private
   */
  onHistoryDeletionDialogClose_: function() {
    this.showHistoryDeletionDialog_ = false;
    this.$.clearBrowsingDataDialog.close();
  },

  /**
   * Records an action when the user changes between the basic and advanced tab.
   * @param {!Event} event
   * @private
   */
  recordTabChange_: function(event) {
    if (event.detail.value == 0) {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_AdvancedTab');
    }
  },

  /**
   * Called when the user clicks the link in the footer.
   * @param {!Event} e
   * @private
   */
  onSyncDescriptionLinkClicked_: function(e) {
    if (e.target.tagName === 'A') {
      e.preventDefault();
      if (!this.syncStatus.hasError) {
        chrome.metricsPrivate.recordUserAction('ClearBrowsingData_Sync_Pause');
        this.syncBrowserProxy_.pauseSync();
      } else if (this.isSyncPaused_) {
        chrome.metricsPrivate.recordUserAction('ClearBrowsingData_Sync_SignIn');
        this.syncBrowserProxy_.startSignIn();
      } else {
        if (this.hasPassphraseError_) {
          chrome.metricsPrivate.recordUserAction(
              'ClearBrowsingData_Sync_NavigateToPassphrase');
        } else {
          chrome.metricsPrivate.recordUserAction(
              'ClearBrowsingData_Sync_NavigateToError');
        }
        // In any other error case, navigate to the sync page.
        settings.navigateTo(settings.routes.SYNC);
      }
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSyncPaused_: function() {
    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction === settings.StatusAction.REAUTHENTICATE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasPassphraseError_: function() {
    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction === settings.StatusAction.ENTER_PASSPHRASE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasOtherError_: function() {
    return this.syncStatus !== undefined && !!this.syncStatus.hasError &&
        !this.isSyncPaused_ && !this.hasPassphraseError_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowFooter_: function() {
    return this.diceEnabled_ && !!this.syncStatus && !!this.syncStatus.signedIn;
  },
});
