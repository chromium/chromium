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
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './history_deletion_dialog.js';
import './passwords_deletion_dialog.js';
import '../controls/settings_checkbox.js';
import '../icons.html.js';
import '../settings_shared.css.js';

import {PrefControlMixinInterface} from '/shared/settings/controls/pref_control_mixin.js';
import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {StatusAction, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsCheckboxElement} from '../controls/settings_checkbox.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {ClearBrowsingDataBrowserProxy, ClearBrowsingDataBrowserProxyImpl, UpdateSyncStateEvent} from './clear_browsing_data_browser_proxy.js';
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
    cookiesCheckboxBasic: SettingsCheckboxElement,
    clearBrowsingDataDialog: CrDialogElement,
    tabs: IronPagesElement,
  };
}

const SettingsClearBrowsingDataDialogElementBase =
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

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
          {value: 0, name: loadTimeData.getString('clearPeriodHour')},
          {value: 1, name: loadTimeData.getString('clearPeriod24Hours')},
          {value: 2, name: loadTimeData.getString('clearPeriod7Days')},
          {value: 3, name: loadTimeData.getString('clearPeriod4Weeks')},
          {value: 4, name: loadTimeData.getString('clearPeriodEverything')},
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

      isChildAccount_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isChildAccount');
        },
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

  // TODO(dpapad): make |syncStatus| private.
  syncStatus: SyncStatus|undefined;
  private counters_: {[k: string]: string};
  private clearFromOptions_: DropdownMenuOptionList;
  private clearingInProgress_: boolean;
  private clearingDataAlertString_: string;
  private clearButtonDisabled_: boolean;
  private isChildAccount_: boolean;
  private showHistoryDeletionDialog_: boolean;
  private showPasswordsDeletionDialogLater_: boolean;
  private showPasswordsDeletionDialog_: boolean;
  private isSignedIn_: boolean;
  private isSyncConsented_: boolean;
  private isSyncingHistory_: boolean;
  private shouldShowCookieException_: boolean;
  private isSyncPaused_: boolean;
  private hasPassphraseError_: boolean;
  private hasOtherSyncError_: boolean;
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
    const tab = this.$.tabs.selectedItem;
    if (!tab) {
      return;
    }
    this.clearButtonDisabled_ =
        this.getSelectedDataTypes_(tab as HTMLElement).length === 0;
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

  /** Choose a label for the cookie checkbox. */
  private cookiesCheckboxLabel_(
      shouldShowCookieException: boolean, cookiesSummary: string,
      cookiesSummarySignedIn: string): string {
    if (shouldShowCookieException) {
      return cookiesSummarySignedIn;
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
  private getSelectedDataTypes_(tab: HTMLElement): string[] {
    const checkboxes = tab.querySelectorAll('settings-checkbox');
    const dataTypes: string[] = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.pref!.key);
      }
    });
    return dataTypes;
  }

  /** Clears browsing data and maybe shows a history notice. */
  private async clearBrowsingData_() {
    this.clearingInProgress_ = true;
    this.clearingDataAlertString_ = loadTimeData.getString('clearingData');
    const tab = this.$.tabs.selectedItem as HTMLElement;
    const dataTypes = this.getSelectedDataTypes_(tab);
    const timePeriod = (tab.querySelector('.time-range-select') as unknown as
                        PrefControlMixinInterface)
                           .pref!.value;

    if (tab.id === 'basic-tab') {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
    }

    this.shadowRoot!
        .querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox[no-set-pref]')
        .forEach(checkbox => checkbox.sendPrefChange());

    const {showHistoryNotice, showPasswordsNotice} =
        await this.browserProxy_.clearBrowsingData(dataTypes, timePeriod);
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

  // <if expr="not chromeos_ash">
  /** Called when the user clicks the link in the footer. */
  private onSyncDescriptionLinkClicked_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      e.preventDefault();
      if (!this.syncStatus!.hasError) {
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
  // </if>

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

  private computeGoogleSearchHistoryString_(isNonGoogleDse: boolean):
      TrustedHTML {
    return isNonGoogleDse ?
        this.i18nAdvanced('clearGoogleSearchHistoryNonGoogleDse') :
        this.i18nAdvanced('clearGoogleSearchHistoryGoogleDse');
  }

  private shouldShowFooter_(): boolean {
    let showFooter = false;
    // <if expr="not is_chromeos">
    showFooter = !!this.syncStatus && !!this.syncStatus!.signedIn;
    // </if>
    return showFooter;
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
