// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_spinner_style.css.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './history_deletion_dialog.js';
import './passwords_deletion_dialog.js';
import '../controls/settings_checkbox.js';
import '../controls/settings_dropdown_menu.js';
import '../icons.html.js';
import '../settings_shared.css.js';
// <if expr="not is_chromeos">
import '../people_page/sync_account_control.js';

// </if>

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
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
import {ClearBrowsingDataBrowserProxyImpl, TimePeriod} from './clear_browsing_data_browser_proxy.js';
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

      // Exposes ChromeSigninAccessPoint enum to HTML bindings.
      accessPointEnum_: {
        type: Object,
        value: ChromeSigninAccessPoint,
      },
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
  declare syncStatus: SyncStatus|undefined;
  declare private counters_: {[k: string]: string};
  declare private clearFromOptions_: DropdownMenuOptionList;
  declare private clearingInProgress_: boolean;
  declare private clearingDataAlertString_: string;
  declare private clearButtonDisabled_: boolean;
  declare private showHistoryDeletionDialog_: boolean;
  declare private showPasswordsDeletionDialogLater_: boolean;
  declare private showPasswordsDeletionDialog_: boolean;
  declare private isSyncingHistory_: boolean;
  declare private shouldShowCookieException_: boolean;
  // <if expr="not is_chromeos">
  declare private isClearPrimaryAccountAllowed_: boolean;
  declare private isSyncPaused_: boolean;
  declare private hasPassphraseError_: boolean;
  declare private hasOtherSyncError_: boolean;
  // </if>
  declare private selectedTabIndex_: number;
  declare private tabsNames_: string[];
  declare private googleSearchHistoryString_: TrustedHTML;
  declare private isNonGoogleDse_: boolean;
  declare private nonGoogleSearchHistoryString_: TrustedHTML;
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
        'browsing-data-counter-text-update',
        this.updateCounterText_.bind(this));

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
    if (signedInState === SignedInState.SIGNED_IN) {
      return clearCookiesSummarySignedIn;
    }

    if (shouldShowCookieException) {
      return clearCookiesSummarySyncing;
    }
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
      case 'basicTab':
        return true;
      case 'advancedTab':
        return false;
      default:
        assertNotReached();
    }
  }

  /** Clears browsing data and maybe shows a history notice. */
  private async clearBrowsingData_() {
    this.clearingInProgress_ = true;
    this.clearingDataAlertString_ = loadTimeData.getString('clearingData');

    const page = this.$.pages.selectedItem as HTMLElement;
    const dataTypes = this.getSelectedDataTypes_(page);
    const dropdownMenu = this.getTimeRangeDropdownForCurrentPage_();
    const timePeriod = Number(dropdownMenu.getSelectedValue());

    if (this.isBasicTabSelected_()) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
      this.browserProxy_
          .recordSettingsClearBrowsingDataBasicTimePeriodHistogram(timePeriod);
    } else {
      // Advanced tab.
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
      this.browserProxy_
          .recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
              timePeriod);
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
        this.syncBrowserProxy_.startSignIn(ChromeSigninAccessPoint.SETTINGS);
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
    return this.syncStatus !== undefined && !!this.syncStatus.hasError &&
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
    if (!this.syncStatus) {
      return false;
    }

    switch (this.syncStatus.signedInState) {
      case SignedInState.SIGNED_IN:
        return this.isClearPrimaryAccountAllowed_;
      case SignedInState.SYNCING:
        return true;
      case SignedInState.WEB_ONLY_SIGNED_IN:
      case SignedInState.SIGNED_OUT:
      case SignedInState.SIGNED_IN_PAUSED:
      default:
        return false;
    }
  }

  /**
   * @return Whether the signed info description should be shown in the footer.
   */
  private showSigninInfo_(): boolean {
    if (!this.syncStatus) {
      return false;
    }

    return this.syncStatus.signedInState === SignedInState.SIGNED_IN &&
        this.isClearPrimaryAccountAllowed_;
  }

  /**
   * @return Whether the synced info description should be shown in the footer.
   */
  private showSyncInfo_(): boolean {
    if (!this.syncStatus) {
      return false;
    }

    return !this.showSigninInfo_() && !this.syncStatus.hasError;
  }
  // </if>

  /**
   * @return Whether the search history box should be shown.
   */
  private showSearchHistoryBox_(): boolean {
    if (!this.syncStatus) {
      return false;
    }

    switch (this.syncStatus.signedInState) {
      case SignedInState.SIGNED_IN_PAUSED:
      case SignedInState.SIGNED_IN:
      case SignedInState.SYNCING:
        return true;
      case SignedInState.WEB_ONLY_SIGNED_IN:
      case SignedInState.SIGNED_OUT:
      default:
        return false;
    }
  }

  private onTimePeriodChanged_() {
    const dropdownMenu = this.getTimeRangeDropdownForCurrentPage_();

    const timePeriod = parseInt(dropdownMenu.getSelectedValue(), 10);
    assert(!Number.isNaN(timePeriod));

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

    if (!this.clearFromOptions_.find(
            timePeriodOption => timePeriodOption.value === timePeriodValue)) {
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
