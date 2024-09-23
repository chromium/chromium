// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './history_deletion_dialog.js';
import './passwords_deletion_dialog.js';
import '../controls/settings_checkbox.js';
import '../icons.html.js';
import '../settings_shared.css.js';
// <if expr="not is_chromeos">
import '../people_page/sync_account_control.js';

// </if>

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCheckboxElement} from '../controls/settings_checkbox.js';
import type {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

import type {ClearBrowsingDataBrowserProxy, UpdateSyncStateEvent} from './clear_browsing_data_browser_proxy.js';
import {ClearBrowsingDataBrowserProxyImpl, TimePeriod, TimePeriodExperiment} from './clear_browsing_data_browser_proxy.js';
import {getTemplate} from './clear_browsing_data_dialog.html.js';

/**
 * @param dialog the dialog to close
 * @param isLast whether this is the last CBD-related dialog
 */
function closeDialog(dialog: CrDialogElement, isLast: boolean) {
  // If this is not the last dialog, then stop the 'close' event from
  // propagating so that other (following) dialogs don't get closed as well.
  if (!isLast) {
    dialog.addEventListener('close', e => {
      e.stopPropagation();
    }, {once: true});
  }
  dialog.close();
}

export interface SettingsClearBrowsingDataDialogElement {
  $: {
    clearBrowsingDataConfirm: HTMLElement,
    cookiesCheckbox: SettingsCheckboxElement,
    cookiesCheckboxBasic: SettingsCheckboxElement,
    clearButton: CrButtonElement,
    clearBrowsingDataDialog: CrDialogElement,
    pages: CrPageSelectorElement,
    tabs: CrTabsElement,
  };
}

const SettingsClearBrowsingDataDialogElementBase = RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsClearBrowsingDataDialogElement extends
    SettingsClearBrowsingDataDialogElementBase {
  static get is() {
    return 'settings-clear-browsing-data-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      /**
       * Results of browsing data counters, keyed by the suffix of
       * the corresponding data type deletion preference, as reported
       * by the C++ side.
       */
      counters_: {
        type: Object,
        // Will be filled as results are reported.
        value() {
          return {};
        },
      },

      /**
       * List of options for the dropdown menu.
       */
      clearFromOptions_: {
        readOnly: true,
        type: Array,
        value: [
          {
            value: TimePeriod.LAST_HOUR,
            name: loadTimeData.getString('clearPeriodHour'),
          },
          {
            value: TimePeriod.LAST_DAY,
            name: loadTimeData.getString('clearPeriod24Hours'),
          },
          {
            value: TimePeriod.LAST_WEEK,
            name: loadTimeData.getString('clearPeriod7Days'),
          },
          {
            value: TimePeriod.FOUR_WEEKS,
            name: loadTimeData.getString('clearPeriod4Weeks'),
          },
          {
            value: TimePeriod.ALL_TIME,
            name: loadTimeData.getString('clearPeriodEverything'),
          },
        ],
      },

      enableCbdTimeframeRequired_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableCbdTimeframeRequired');
        },
      },

      unoDesktopEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('unoDesktopEnabled');
        },
      },

      /**
       * When CBDTimeframeRequired feature/flag is on, this will be the list
       * of options for the dropdown menu. V2 additionally contains the "Last 15
       * minutes" and the "Select a time range" options with "Select a time
       * range" being always hidden in the menuOptions list in which users can
       * chose the time range.
       */
      clearFromOptionsV2_: {
        readOnly: true,
        type: Array,
        value: [
          // The pref is initialized to TimePeriodExperiment.NOT_SELECTED, which
          // is shown in the dropdown as the selected option until the user
          // selects a different value. The menuList of options should not
          // contain the option for TimePeriodExperiment.NOT_SELECTED, as it
          // doesn't make sense for users to choose it.
          {
            value: TimePeriodExperiment.NOT_SELECTED,
            name: loadTimeData.getString('clearPeriodNotSelected'),
            hidden: true,
          },
          // The value of 15min is 6 to match the value written in the backend,
          // Also, it comes first in the list to keep the list in ascending
          // order.
          {
            value: TimePeriodExperiment.LAST_15_MINUTES,
            name: loadTimeData.getString('clearPeriod15Minutes'),
          },
          {
            value: TimePeriodExperiment.LAST_HOUR,
            name: loadTimeData.getString('clearPeriodHour'),
          },
          {
            value: TimePeriodExperiment.LAST_DAY,
            name: loadTimeData.getString('clearPeriod24Hours'),
          },
          {
            value: TimePeriodExperiment.LAST_WEEK,
            name: loadTimeData.getString('clearPeriod7Days'),
          },
          {
            value: TimePeriodExperiment.FOUR_WEEKS,
            name: loadTimeData.getString('clearPeriod4Weeks'),
          },
          {
            value: TimePeriodExperiment.ALL_TIME,
            name: loadTimeData.getString('clearPeriodEverything'),
          },
        ],
      },

      clearingInProgress_: {
        type: Boolean,
        value: false,
      },

      clearingDataAlertString_: {
        type: String,
        value: '',
      },

      clearButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      showHistoryDeletionDialog_: {
        type: Boolean,
        value: false,
      },

      showPasswordsDeletionDialogLater_: {
        type: Boolean,
        value: false,
      },

      showPasswordsDeletionDialog_: {
        type: Boolean,
        value: false,
      },

      isSignedIn_: {
        type: Boolean,
        value: false,
      },

      isSyncConsented_: {
        type: Boolean,
        value: false,
      },

      isSyncingHistory_: {
        type: Boolean,
        value: false,
      },

      shouldShowCookieException_: {
        type: Boolean,
        value: false,
      },

      // <if expr="not is_chromeos">
      isClearPrimaryAccountAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isClearPrimaryAccountAllowed');
        },
      },

      isSyncPaused_: {
        type: Boolean,
        value: false,
        computed: 'computeIsSyncPaused_(syncStatus)',
      },

      hasPassphraseError_: {
        type: Boolean,
        value: false,
        computed: 'computeHasPassphraseError_(syncStatus)',
      },

      hasOtherSyncError_: {
        type: Boolean,
        value: false,
        computed:
            'computeHasOtherError_(syncStatus, isSyncPaused_, hasPassphraseError_)',
      },
      // </if>

      selectedTabIndex_: Number,

      tabsNames_: {
        type: Array,
        value: () =>
            [loadTimeData.getString('basicPageTitle'),
             loadTimeData.getString('advancedPageTitle'),
    ],
      },

      googleSearchHistoryString_: {
        type: String,
        computed: 'computeGoogleSearchHistoryString_(isNonGoogleDse_)',
      },

      isNonGoogleDse_: {
        type: Boolean,
        value: false,
      },

      nonGoogleSearchHistoryString_: String,
    };
  }

  static get observers() {
    return [
      `onTimePeriodAdvancedPrefUpdated_(
          prefs.browser.clear_data.time_period.value)`,
      `onTimePeriodBasicPrefUpdated_(
          prefs.browser.clear_data.time_period_basic.value)`,
      `onSelectedTabIndexPrefUpdated_(
          prefs.browser.last_clear_browsing_data_tab.value)`,
    ];
  }

  // TODO(dpapad): make |syncStatus| private.
  syncStatus: SyncStatus|undefined;
  private counters_: {[k: string]: string};
  private clearFromOptions_: DropdownMenuOptionList;
  private clearFromOptionsV2_: DropdownMenuOptionList;
  private enableCbdTimeframeRequired_: boolean;
  private unoDesktopEnabled_: boolean;
  private clearingInProgress_: boolean;
  private clearingDataAlertString_: string;
  private clearButtonDisabled_: boolean;
  private showHistoryDeletionDialog_: boolean;
  private showPasswordsDeletionDialogLater_: boolean;
  private showPasswordsDeletionDialog_: boolean;
  private isSignedIn_: boolean;
  private isSyncConsented_: boolean;
  private isSyncingHistory_: boolean;
  private shouldShowCookieException_: boolean;
  // <if expr="not is_chromeos">
  private isClearPrimaryAccountAllowed_: boolean;
  private isSyncPaused_: boolean;
  private hasPassphraseError_: boolean;
  private hasOtherSyncError_: boolean;
  // </if>
  private selectedTabIndex_: number;
  private tabsNames_: string[];
  private googleSearchHistoryString_: TrustedHTML;
  private isNonGoogleDse_: boolean;
  private nonGoogleSearchHistoryString_: TrustedHTML;
  private focusOutlineManager_: FocusOutlineManager;

  private browserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    this.addWebUiListener(
        'update-sync-state', this.updateSyncState_.bind(this));
    this.addWebUiListener(
        'update-counter-text', this.updateCounterText_.bind(this));

    this.addEventListener(
        'settings-boolean-control-change', this.updateClearButtonState_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.initialize().then(() => {
      this.$.clearBrowsingDataDialog.showModal();

      // AutoFocus is not visible in mouse navigation by default. But in this
      // dialog the default focus is on cancel which is not a default button. To
      // make this clear to the user we make it visible to the user and remove
      // the focus after the next mouse event.
      this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);

      this.focusOutlineManager_.visible = true;
      document.addEventListener('mousedown', () => {
        this.focusOutlineManager_.visible = false;
      }, {once: true});
    });
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus) {
    this.syncStatus = syncStatus;
  }

  /**
   * @return Whether either clearing is in progress or no data type is selected.
   */
  private isClearButtonDisabled_(
      clearingInProgress: boolean, clearButtonDisabled: boolean): boolean {
    return clearingInProgress || clearButtonDisabled;
  }

  /**
   * Disables the Clear Data button if no data type is selected.
   */
  private updateClearButtonState_() {
    // on-select-item-changed gets called with undefined during a tab change.
    // https://github.com/PolymerElements/iron-selector/issues/95
    const page = this.$.pages.selectedItem;
    if (!page) {
      return;
    }
    this.clearButtonDisabled_ =
        this.getSelectedDataTypes_(page as HTMLElement).length === 0;
  }

  /**
   * Record visits to the CBD dialog.
   *
   * RouteObserverMixin
   */
  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute === routes.CLEAR_BROWSER_DATA) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_DialogCreated');
    }
  }

  /**
   * Updates the history description to show the relevant information
   * depending on sync and signin state.
   */
  private updateSyncState_(event: UpdateSyncStateEvent) {
    this.isSignedIn_ = event.signedIn;
    this.isSyncConsented_ = event.syncConsented;
    this.isSyncingHistory_ = event.syncingHistory;
    this.shouldShowCookieException_ = event.shouldShowCookieException;
    this.$.clearBrowsingDataDialog.classList.add('fully-rendered');
    this.isNonGoogleDse_ = event.isNonGoogleDse;
    this.nonGoogleSearchHistoryString_ =
        sanitizeInnerHtml(event.nonGoogleSearchHistoryString);
  }

  /** Choose a label for the history checkbox. */
  private browsingCheckboxLabel_(
      isSyncingHistory: boolean, historySummary: string,
      historySummarySignedInNoLink: string): string {
    return isSyncingHistory ? historySummarySignedInNoLink : historySummary;
  }

  /**
   * Choose a label for the cookie checkbox
   * @param signedInState SignedInState
   * @param shouldShowCookieException boolean whether the exception about not
   * being signed out of your Google account should be shown when user is
   * sync.
   * @param cookiesSummary string explaining that deleting cookies and site data
   * will sign the user out of most websites.
   * @param clearCookiesSummarySignedIn string explaining that deleting cookies
   * and site data will sign the user out of most websites but Google sign in
   * will stay.
   * @param clearCookiesSummarySyncing string explaining that deleting cookies
   * and site data will sign the user out of most websites but Google sign in
   * will stay when user is syncing.
   * @param clearCookiesSummarySignedInSupervisedProfile string used for a
   * supervised user. Gives information about family link controls and that they
   * will not be signed out on clearing cookies
   */
  private cookiesCheckboxLabel_(
      signedInState: SignedInState,
      shouldShowCookieException: boolean,
      cookiesSummary: string,
      clearCookiesSummarySignedIn: string,
      clearCookiesSummarySyncing: string,
      // <if expr="is_linux or is_macosx or is_win">
      clearCookiesSummarySignedInSupervisedProfile: string,
      // </if>
      ): string {
    // <if expr="is_linux or is_macosx or is_win">
    if (loadTimeData.getBoolean('isChildAccount')) {
      return clearCookiesSummarySignedInSupervisedProfile;
    }
    // </if>

    // The exception is not shown for SIGNED_IN_PAUSED.
    if (this.unoDesktopEnabled_ && signedInState === SignedInState.SIGNED_IN) {
      return clearCookiesSummarySignedIn;
    }

    if (shouldShowCookieException) {
      return clearCookiesSummarySyncing;
    }
    // <if expr="chromeos_lacros">
    if (!loadTimeData.getBoolean('isSecondaryUser')) {
      return loadTimeData.getString('clearCookiesSummarySignedInMainProfile');
    }
    // </if>
    return cookiesSummary;
  }

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param prefName Browsing data type deletion preference.
   * @param text The text with which to update the counter
   */
  private updateCounterText_(prefName: string, text: string) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    const matches = prefName.match(/^browser\.clear_data\.(\w+)$/)!;
    assert(matches[1]);
    this.set('counters_.' + matches[1], text);
  }

  /**
   * @return A list of selected data types.
   */
  private getSelectedDataTypes_(page: HTMLElement): string[] {
    const checkboxes = page.querySelectorAll('settings-checkbox');
    const dataTypes: string[] = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.pref!.key);
      }
    });
    return dataTypes;
  }

  private getTimeRangeDropdownForCurrentPage_() {
    const page = this.$.pages.selectedItem as HTMLElement;
    const dropdownMenu =
        page.querySelector<SettingsDropdownMenuElement>('.time-range-select');
    assert(dropdownMenu);
    return dropdownMenu;
  }

  private isBasicTabSelected_() {
    const page = this.$.pages.selectedItem as HTMLElement;
    assert(page);
    switch (page.id) {
      case 'basic-tab':
        return true;
      case 'advanced-tab':
        return false;
      default:
        assertNotReached();
    }
  }

  // TODO(crbug.com/40283307): Remove this after CbdTimeframeRequired finishes.
  /** Highlight the time period dropdown in case no selection was made. */
  private validateSelectedTimeRange_(): boolean {
    const dropdownMenu = this.getTimeRangeDropdownForCurrentPage_();
    const timePeriod = Number(dropdownMenu.getSelectedValue());
    if (timePeriod !== TimePeriodExperiment.NOT_SELECTED) {
      return true;
    }
    // No time period is selected: the time period dropdown gets highlighted,
    // and no clearing should happen.
    dropdownMenu.classList.add('dropdown-error');
    // Move the focus to the dropdown. This visually indicates the requirement
    // to select a time period, which the dropdown clarifies via the text of its
    // current selection. This also allows screen readers to read out this text
    // to a11y users to indicate this requirement to them.
    dropdownMenu.focus();
    return false;
  }

  // TODO(crbug.com/40283307): Remove once crbug.com/1487530 completed.
  private cbdExperimentDualWritePrefs_() {
    // To avoid in- and out-of-experiment prefs of the CBD time range experiment
    // (crbug.com/1487530) from diverging, the in-experiment prefs should also
    // be written to the out-of-experiment prefs. A 15min in-experiment
    // selection should be a 1h out-of-experiment selection. Out-of-experiment
    // prefs should also be written to the in-experiment prefs iff the in-
    // experiment prefs value is not TimePeriodExperiment.NOT_SELECTED.
    const dropdownMenuBasic =
        this.shadowRoot!.querySelector<SettingsCheckboxElement>(
            '#clearFromBasic');
    assert(dropdownMenuBasic);
    const timeRangeBasic =
        dropdownMenuBasic.pref!.value === TimePeriodExperiment.LAST_15_MINUTES ?
        TimePeriod.LAST_HOUR :
        dropdownMenuBasic.pref!.value;

    const dropdownMenuAdvanced =
        this.shadowRoot!.querySelector<SettingsCheckboxElement>('#clearFrom');
    assert(dropdownMenuAdvanced);
    const timeRangeAdvanced = dropdownMenuAdvanced.pref!.value ===
            TimePeriodExperiment.LAST_15_MINUTES ?
        TimePeriod.LAST_HOUR :
        dropdownMenuAdvanced.pref!.value;

    if (this.enableCbdTimeframeRequired_) {
      this.setPrefValue('browser.clear_data.time_period_basic', timeRangeBasic);
      this.setPrefValue('browser.clear_data.time_period', timeRangeAdvanced);
    } else {
      // Out-of-experiment.
      if (this.getPref('browser.clear_data.time_period_v2_basic').value !==
          TimePeriodExperiment.NOT_SELECTED) {
        this.setPrefValue(
            'browser.clear_data.time_period_v2_basic', timeRangeBasic);
      }
      if (this.getPref('browser.clear_data.time_period_v2').value !==
          TimePeriodExperiment.NOT_SELECTED) {
        this.setPrefValue(
            'browser.clear_data.time_period_v2', timeRangeAdvanced);
      }
    }
  }

  /** Clears browsing data and maybe shows a history notice. */
  private async clearBrowsingData_() {
    if (!this.validateSelectedTimeRange_()) {
      return;
    }

    this.clearingInProgress_ = true;
    this.clearingDataAlertString_ = loadTimeData.getString('clearingData');

    const page = this.$.pages.selectedItem as HTMLElement;
    const dataTypes = this.getSelectedDataTypes_(page);
    const dropdownMenu = this.getTimeRangeDropdownForCurrentPage_();
    const timePeriod = Number(dropdownMenu.getSelectedValue());

    if (this.isBasicTabSelected_()) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
      // For users in the CbdTimeframeRequired experiment, the selection should
      // only be recorded the first time they clear data. This needs to be
      // checked before the selected time range is written to prefs.
      if (!this.enableCbdTimeframeRequired_ ||
          this.getPref<TimePeriodExperiment>(
                  'browser.clear_data.time_period_v2_basic')
                  .value === TimePeriodExperiment.NOT_SELECTED) {
        this.browserProxy_
            .recordSettingsClearBrowsingDataBasicTimePeriodHistogram(
                timePeriod);
      }
    } else {
      // Advanced tab.
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
      // For users in the CbdTimeframeRequired experiment, the selection should
      // only be recorded the first time they clear data. This needs to be
      // checked before the selected time range is written to prefs.
      if (!this.enableCbdTimeframeRequired_ ||
          this.getPref<TimePeriodExperiment>(
                  'browser.clear_data.time_period_v2')
                  .value === TimePeriodExperiment.NOT_SELECTED) {
        this.browserProxy_
            .recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
                timePeriod);
      }
    }

    this.setPrefValue(
        'browser.last_clear_browsing_data_tab', this.selectedTabIndex_);
    // Dropdown menu and checkbox selections of both tabs should be persisted
    // independently from the tab on which the user confirmed the deletion.
    this.shadowRoot!
        .querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox[no-set-pref]')
        .forEach(checkbox => checkbox.sendPrefChange());
    this.shadowRoot!
        .querySelectorAll<SettingsDropdownMenuElement>(
            'settings-dropdown-menu[no-set-pref]')
        .forEach(dropdown => dropdown.sendPrefChange());

    // Dual write prefs only after the regular prefs have been written above.
    this.cbdExperimentDualWritePrefs_();

    const {showHistoryNotice, showPasswordsNotice} =
        await this.browserProxy_.clearBrowsingData(
            dataTypes, dropdownMenu.pref!.value);
    this.clearingInProgress_ = false;
    getAnnouncerInstance().announce(loadTimeData.getString('clearedData'));
    this.showHistoryDeletionDialog_ = showHistoryNotice;
    // If both the history notice and the passwords notice should be shown, show
    // the history notice first, and then show the passwords notice once the
    // history notice gets closed.
    this.showPasswordsDeletionDialog_ =
        showPasswordsNotice && !showHistoryNotice;
    this.showPasswordsDeletionDialogLater_ =
        showPasswordsNotice && showHistoryNotice;

    // Close the clear browsing data if it is open.
    const isLastDialog = !showHistoryNotice && !showPasswordsNotice;
    if (this.$.clearBrowsingDataDialog.open) {
      closeDialog(this.$.clearBrowsingDataDialog, isLastDialog);
    }
  }

  private onCancelClick_() {
    this.$.clearBrowsingDataDialog.cancel();
  }

  /**
   * Handles the closing of the notice about other forms of browsing history.
   */
  private onHistoryDeletionDialogClose_(e: Event) {
    this.showHistoryDeletionDialog_ = false;
    if (this.showPasswordsDeletionDialogLater_) {
      // Stop the close event from propagating further and also automatically
      // closing other dialogs.
      e.stopPropagation();
      this.showPasswordsDeletionDialogLater_ = false;
      this.showPasswordsDeletionDialog_ = true;
    }
  }

  /**
   * Handles the closing of the notice about incomplete passwords deletion.
   */
  private onPasswordsDeletionDialogClose_() {
    this.showPasswordsDeletionDialog_ = false;
  }

  private onSelectedTabIndexPrefUpdated_(selectedTabIndex: number) {
    this.selectedTabIndex_ = selectedTabIndex;
  }

  /**
   * Records an action when the user changes between the basic and advanced tab.
   */
  private recordTabChange_(event: CustomEvent<{value: number}>) {
    if (event.detail.value === 0) {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_AdvancedTab');
    }
  }

  // <if expr="not is_chromeos">
  /** Called when the user clicks the link in the footer. */
  private onSyncDescriptionLinkClicked_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      e.preventDefault();
      if (this.showSigninInfo_()) {
        chrome.metricsPrivate.recordUserAction('ClearBrowsingData_SignOut');
        this.syncBrowserProxy_.signOut(/*delete_profile=*/ false);
      } else if (this.showSyncInfo_()) {
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
        Router.getInstance().navigateTo(routes.SYNC);
      }
    }
  }

  private computeIsSyncPaused_(): boolean {
    return !!this.syncStatus!.hasError &&
        !this.syncStatus!.hasUnrecoverableError &&
        this.syncStatus!.statusAction === StatusAction.REAUTHENTICATE;
  }

  private computeHasPassphraseError_(): boolean {
    return !!this.syncStatus!.hasError &&
        this.syncStatus!.statusAction === StatusAction.ENTER_PASSPHRASE;
  }

  private computeHasOtherError_(): boolean {
    return this.syncStatus !== undefined && !!this.syncStatus!.hasError &&
        !this.isSyncPaused_ && !this.hasPassphraseError_;
  }
  // </if>

  private computeGoogleSearchHistoryString_(isNonGoogleDse: boolean):
      TrustedHTML {
    return isNonGoogleDse ?
        this.i18nAdvanced('clearGoogleSearchHistoryNonGoogleDse') :
        this.i18nAdvanced('clearGoogleSearchHistoryGoogleDse');
  }

  // <if expr="not is_chromeos">
  private shouldShowFooter_(): boolean {
    if (!!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SYNCING) {
      return true;
    }
    return this.unoDesktopEnabled_ && this.isClearPrimaryAccountAllowed_ &&
        this.isSignedIn_;
  }

  /**
   * @return Whether the signed info description should be shown in the footer.
   */
  private showSigninInfo_(): boolean {
    return this.unoDesktopEnabled_ && this.isSignedIn_ &&
        this.isClearPrimaryAccountAllowed_ &&
        (!this.syncStatus ||
         this.syncStatus.signedInState !== SignedInState.SYNCING);
  }

  /**
   * @return Whether the synced info description should be shown in the footer.
   */
  private showSyncInfo_(): boolean {
    return !this.showSigninInfo_() && !!this.syncStatus &&
        !this.syncStatus.hasError;
  }
  // </if>

  private onTimePeriodChanged_() {
    const dropdownMenu = this.getTimeRangeDropdownForCurrentPage_();

    // Needed in the |enableCbdTimeframeRequired_| experiment, no-op otherwise.
    // TODO(crbug.com/40283307): Remove when crbug.com/1487530 finished.
    dropdownMenu.classList.remove('dropdown-error');

    let timePeriod = parseInt(dropdownMenu.getSelectedValue(), 10);
    assert(!Number.isNaN(timePeriod));

    // If the time period is not selected, count all the data.
    if (timePeriod === TimePeriodExperiment.NOT_SELECTED) {
      timePeriod = TimePeriodExperiment.ALL_TIME;
    }

    this.browserProxy_.restartCounters(this.isBasicTabSelected_(), timePeriod);
  }

  private onTimePeriodAdvancedPrefUpdated_() {
    this.onTimePeriodPrefUpdated_(false);
  }

  private onTimePeriodBasicPrefUpdated_() {
    this.onTimePeriodPrefUpdated_(true);
  }


  private onTimePeriodPrefUpdated_(isBasic: boolean) {
    const timePeriodPref = isBasic ? 'browser.clear_data.time_period_basic' :
                                     'browser.clear_data.time_period';

    const timePeriodValue = this.getPref(timePeriodPref).value;

    if (!(timePeriodValue in TimePeriod)) {
      // If the synced time period is not supported, default to "Last hour".
      this.setPrefValue(timePeriodPref, TimePeriod.LAST_HOUR);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-dialog':
        SettingsClearBrowsingDataDialogElement;
  }
}

customElements.define(
    SettingsClearBrowsingDataDialogElement.is,
    SettingsClearBrowsingDataDialogElement);
