// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_checkbox.js';
// <if expr="not is_chromeos">
import './clear_browsing_data_account_indicator.js';
// </if>
import './clear_browsing_data_time_picker.js';
import './history_deletion_dialog.js';
import './other_google_data_dialog.js';

import type {SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCheckboxElement} from '../controls/settings_checkbox.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';

import type {ClearBrowsingDataBrowserProxy, UpdateSyncStateEvent} from './clear_browsing_data_browser_proxy.js';
import {BrowsingDataType, ClearBrowsingDataBrowserProxyImpl, TimePeriod} from './clear_browsing_data_browser_proxy.js';
import {getCss} from './clear_browsing_data_dialog.css.js';
import {getHtml} from './clear_browsing_data_dialog.html.js';
import {canDeleteAccountData, isSignedIn} from './clear_browsing_data_signin_util.js';
import type {SettingsClearBrowsingDataTimePickerElement} from './clear_browsing_data_time_picker.js';
import {getTimePeriodString} from './clear_browsing_data_time_picker.js';

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
    cancelButton: CrButtonElement,
    deleteButton: CrButtonElement,
    deleteBrowsingDataDialog: CrDialogElement,
    deletingDataAlert: HTMLElement,
    manageOtherGoogleDataRow: CrLinkRowElement,
    moreOptionsList: HTMLElement,
    showMoreButton: CrButtonElement,
    spinner: HTMLElement,
    timePicker: SettingsClearBrowsingDataTimePickerElement,
  };
}

/**
 * The list of all available Browsing Data types in the default order they
 * should appear in the dialog.
 */
const ALL_BROWSING_DATATYPES_LIST: BrowsingDataType[] = [
  BrowsingDataType.HISTORY,
  BrowsingDataType.SITE_DATA,
  BrowsingDataType.CACHE,
  BrowsingDataType.DOWNLOADS,
  BrowsingDataType.FORM_DATA,
  BrowsingDataType.SITE_SETTINGS,
  BrowsingDataType.HOSTED_APPS_DATA,
];

/** The list of Browsing Data types that should be expanded by default. */
const DEFAULT_BROWSING_DATATYPES_LIST: BrowsingDataType[] = [
  BrowsingDataType.HISTORY,
  BrowsingDataType.SITE_DATA,
  BrowsingDataType.CACHE,
];

interface BrowsingDataTypeOption {
  label: string;
  subLabel?: string;
  prefKey: string;
}

function getDataTypeLabel(datatypes: BrowsingDataType) {
  switch (datatypes) {
    case BrowsingDataType.HISTORY:
      return loadTimeData.getString('clearBrowsingHistory');
    case BrowsingDataType.CACHE:
      return loadTimeData.getString('clearCache');
    case BrowsingDataType.SITE_DATA:
      return loadTimeData.getString('clearCookies');
    case BrowsingDataType.FORM_DATA:
      return loadTimeData.getString('clearFormData');
    case BrowsingDataType.SITE_SETTINGS:
      return loadTimeData.getString('siteSettings');
    case BrowsingDataType.DOWNLOADS:
      return loadTimeData.getString('clearDownloadHistory');
    case BrowsingDataType.HOSTED_APPS_DATA:
      return loadTimeData.getString('clearHostedAppData');
    default:
      assertNotReachedCase(datatypes);
  }
}

export function getDataTypePrefName(datatypes: BrowsingDataType) {
  switch (datatypes) {
    case BrowsingDataType.HISTORY:
      return 'browser.clear_data.browsing_history';
    case BrowsingDataType.CACHE:
      return 'browser.clear_data.cache';
    case BrowsingDataType.SITE_DATA:
      return 'browser.clear_data.cookies';
    case BrowsingDataType.FORM_DATA:
      return 'browser.clear_data.form_data';
    case BrowsingDataType.SITE_SETTINGS:
      return 'browser.clear_data.site_settings';
    case BrowsingDataType.DOWNLOADS:
      return 'browser.clear_data.download_history';
    case BrowsingDataType.HOSTED_APPS_DATA:
      return 'browser.clear_data.hosted_apps_data';
    default:
      assertNotReachedCase(datatypes);
  }
}

const SettingsClearBrowsingDataDialogElementBase =
    RouteObserverMixinLit(WebUiListenerMixinLit(CrLitElement));

export type ClearBrowsingDataDialogElement =
    SettingsClearBrowsingDataDialogElement;

export class SettingsClearBrowsingDataDialogElement extends
    SettingsClearBrowsingDataDialogElementBase {
  static get is() {
    return 'settings-clear-browsing-data-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataTypesExpanded_: {type: Boolean},
      deletingDataAlertString_: {type: String},
      isDeletionInProgress_: {type: Boolean},
      isNoDatatypeSelected_: {type: Boolean},
      isGoogleDse_: {type: Boolean},
      showHistoryDeletionDialog_: {type: Boolean},
      showOtherGoogleDataDialog_: {type: Boolean},
      expandedBrowsingDataTypeOptionsList_: {type: Array},
      moreBrowsingDataTypeOptionsList_: {type: Array},
      syncStatus_: {type: Object},
    };
  }

  protected accessor dataTypesExpanded_: boolean = false;
  protected accessor deletingDataAlertString_: string = '';
  protected accessor isDeletionInProgress_: boolean = false;
  private accessor isNoDatatypeSelected_: boolean = false;
  private accessor isGoogleDse_: boolean = false;
  protected accessor showHistoryDeletionDialog_: boolean = false;
  protected accessor showOtherGoogleDataDialog_: boolean = false;
  protected accessor expandedBrowsingDataTypeOptionsList_:
      BrowsingDataTypeOption[] = [];
  protected accessor moreBrowsingDataTypeOptionsList_:
      BrowsingDataTypeOption[] = [];
  private accessor syncStatus_: SyncStatus|undefined;

  private clearBrowsingDataBrowserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.clearBrowsingDataBrowserProxy_.initialize();

    this.setFocusOutlineToVisible_();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.addWebUiListener(
        'browsing-data-counter-text-update',
        this.updateCounterText_.bind(this));

    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));

    this.addEventListener(
        'settings-boolean-control-change',
        this.updateDeleteButtonState_.bind(this));

    this.addWebUiListener(
        'update-sync-state',
        (event: UpdateSyncStateEvent) =>
            this.updateDseStatus_(event.isNonGoogleDse));
    this.clearBrowsingDataBrowserProxy_.getSyncState().then(
        (event: UpdateSyncStateEvent) =>
            this.updateDseStatus_(event.isNonGoogleDse));

    PrefService.getInstance().whenInitialized().then(() => {
      this.setUpDataTypeOptionLists_();
      // Wait for checkbox lists to be populated before checking if the delete
      // button should be disabled.
      this.updateComplete.then(() => this.updateDeleteButtonState_());
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('syncStatus_')) {
      this.onSyncStatusChanged_();
    }
  }

  private updateDseStatus_(isNonGoogleDse: boolean) {
    this.isGoogleDse_ = !isNonGoogleDse;
  }

  private handleSyncStatus_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;
  }

  private onSyncStatusChanged_() {
    if (this.syncStatus_ === undefined) {
      return;
    }

    this.clearBrowsingDataBrowserProxy_.restartCounters(
        this.$.timePicker.getSelectedTimePeriod());
  }

  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute === routes.CLEAR_BROWSER_DATA) {
      this.metricsBrowserProxy_.recordAction('ClearBrowsingData_DialogCreated');
    }
  }

  private setUpDataTypeOptionLists_() {
    const expandedOptionsList: BrowsingDataTypeOption[] = [];
    const moreOptionsList: BrowsingDataTypeOption[] = [];

    ALL_BROWSING_DATATYPES_LIST.forEach((datatype) => {
      const datatypeOption: BrowsingDataTypeOption = {
        label: getDataTypeLabel(datatype),
        prefKey: getDataTypePrefName(datatype),
      };

      if (this.shouldDataTypeBeExpanded_(datatype)) {
        expandedOptionsList.push(datatypeOption);
      } else {
        moreOptionsList.push(datatypeOption);
      }
    });

    this.expandedBrowsingDataTypeOptionsList_ = expandedOptionsList;
    this.moreBrowsingDataTypeOptionsList_ = moreOptionsList;
  }

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param prefName Browsing data type deletion preference.
   * @param text The text with which to update the counter.
   */
  private async updateCounterText_(prefName: string, text: string) {
    await PrefService.getInstance().whenInitialized();

    // If the corresponding datatype is in the expanded options list, update the
    // sub-label.
    const expandedListIndex =
        this.expandedBrowsingDataTypeOptionsList_.map(option => option.prefKey)
            .indexOf(prefName);
    if (expandedListIndex !== -1) {
      const item = this.expandedBrowsingDataTypeOptionsList_[expandedListIndex];
      this.expandedBrowsingDataTypeOptionsList_ =
          this.expandedBrowsingDataTypeOptionsList_.toSpliced(
              expandedListIndex, 1, {...item, subLabel: text});
      return;
    }

    // If the datatype is not found in the expanded options list, it should be
    // in the more options list.
    const moreListIndex =
        this.moreBrowsingDataTypeOptionsList_.map(option => option.prefKey)
            .indexOf(prefName);
    assert(moreListIndex !== -1);
    const moreItem = this.moreBrowsingDataTypeOptionsList_[moreListIndex];
    this.moreBrowsingDataTypeOptionsList_ =
        this.moreBrowsingDataTypeOptionsList_.toSpliced(
            moreListIndex, 1, {...moreItem, subLabel: text});
  }

  private isSignedIn_() {
    return isSignedIn(this.syncStatus_);
  }

  private shouldDataTypeBeExpanded_(datatype: BrowsingDataType) {
    return DEFAULT_BROWSING_DATATYPES_LIST.includes(datatype) ||
        PrefService.getInstance()
            .getPref<boolean>(getDataTypePrefName(datatype))
            .value;
  }

  protected computeDeleteButtonLabel_() {
    return canDeleteAccountData(this.syncStatus_) ?
        loadTimeData.getString('clearData') :
        loadTimeData.getString('deleteDataFromDevice');
  }

  protected computeOtherGoogleDataRowLabel_() {
    return this.isGoogleDse_ ?
        loadTimeData.getString('manageOtherGoogleDataLabel') :
        loadTimeData.getString('manageOtherDataLabel');
  }

  protected computeOtherGoogleDataRowSubLabel_() {
    if (loadTimeData.getBoolean('showGlicSettings') && this.isSignedIn_()) {
      return loadTimeData.getString('manageSearchGeminiPasswordsSubLabel');
    }

    if (this.isSignedIn_() || !this.isGoogleDse_) {
      return loadTimeData.getString('manageOtherDataSubLabel');
    }

    return loadTimeData.getString('managePasswordsSubLabel');
  }

  protected onSelectedTimePeriodChange_() {
    this.clearBrowsingDataBrowserProxy_.restartCounters(
        this.$.timePicker.getSelectedTimePeriod());
  }

  protected onCancelClick_() {
    this.$.deleteBrowsingDataDialog.close();
  }

  /**
   * Triggers browsing data deletion on the selected DataTypes and within the
   * selected TimePeriod.
   */
  protected async onDeleteBrowsingDataClick_() {
    this.deletingDataAlertString_ = loadTimeData.getString('clearingData');
    this.isDeletionInProgress_ = true;

    const dataTypes = this.getSelectedDataTypes_();
    const timePeriod = this.$.timePicker.getSelectedTimePeriod();
    this.clearBrowsingDataBrowserProxy_
        .recordSettingsClearBrowsingDataTimePeriodHistogram(timePeriod);

    // Update the DataType and TimePeriod prefs with the latest selection.
    const prefService = PrefService.getInstance();
    this.$.deleteBrowsingDataDialog
        .querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox[no-set-pref]')
        .forEach(
            checkbox =>
                // Manually update the checkboxes' pref value. This is a
                // temporary fix as the `SettingsCheckbox.sendPrefChange` does
                // not update prefs when they are passed dynamically.
                // TODO(crbug.com/431174247): Figure out why
                // `SettingsCheckbox.sendPrefChange` is not working.
            prefService.setPrefValue(checkbox.prefKey, checkbox.checked));
    this.$.timePicker.sendPrefChange();

    const {showHistoryNotice} =
        await this.clearBrowsingDataBrowserProxy_.clearBrowsingData(
            dataTypes, timePeriod);
    this.isDeletionInProgress_ = false;
    this.showHistoryDeletionDialog_ = showHistoryNotice;
    this.showDeletionConfirmationToast_(timePeriod);

    if (this.$.deleteBrowsingDataDialog.open) {
      closeDialog(this.$.deleteBrowsingDataDialog, !showHistoryNotice);
    }
  }

  private showDeletionConfirmationToast_(timePeriod: TimePeriod) {
    const deletionConfirmationToastLabel = timePeriod === TimePeriod.ALL_TIME ?
        loadTimeData.getString('deletionConfirmationAllTimeToast') :
        loadTimeData.getStringF(
            'deletionConfirmationToast',
            getTimePeriodString(timePeriod, /*short=*/ false));

    this.fire('browsing-data-deleted', {
      deletionConfirmationText: deletionConfirmationToastLabel,
    });
  }

  private getSelectedDataTypes_(): string[] {
    // Get all the visible checkboxes in the dialog. Hidden checkboxes, eg.
    // collapsed checkboxes in the 'More' list, would never be selected, so
    // there is no need to iterate over them.
    const checkboxes =
        this.$.deleteBrowsingDataDialog.querySelectorAll('settings-checkbox');
    const dataTypes: string[] = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.prefKey);
      }
    });
    return dataTypes;
  }

  private updateDeleteButtonState_() {
    this.isNoDatatypeSelected_ = this.getSelectedDataTypes_().length === 0;
  }

  protected onShowMoreClick_() {
    this.dataTypesExpanded_ = true;
    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.CheckboxesShowMoreClick');

    // Set the focus to the first checkbox in the 'more' options list.
    this.updateComplete.then(() => {
      const toFocus = this.$.moreOptionsList.querySelector('settings-checkbox');
      assert(toFocus);
      toFocus.focus();
    });
  }

  protected shouldHideShowMoreButton_(): boolean {
    return this.dataTypesExpanded_ ||
        this.moreBrowsingDataTypeOptionsList_.length === 0;
  }

  protected shouldDisableDeleteButton_(): boolean {
    return this.isDeletionInProgress_ || this.isNoDatatypeSelected_;
  }

  protected onHistoryDeletionDialogClose_() {
    this.showHistoryDeletionDialog_ = false;
  }

  protected onManageOtherGoogleDataRowClick_() {
    this.showOtherGoogleDataDialog_ = true;
    this.metricsBrowserProxy_.recordAction(
        'Settings.DeleteBrowsingData.OtherDataEntryPointClick');
  }

  private setFocusOutlineToVisible_() {
    // AutoFocus is not visible in mouse navigation by default. But in this
    // dialog the default focus is on cancel which is not a default button. To
    // make this clear to the user we make it visible to the user and remove
    // the focus after the next mouse event.
    const focusOutlineManager = FocusOutlineManager.forDocument(document);
    focusOutlineManager.visible = true;

    document.addEventListener('mousedown', () => {
      focusOutlineManager.visible = false;
    }, {once: true});
  }

  protected onOtherGoogleDataDialogCancel_(e: Event) {
    e.stopPropagation();
    this.showOtherGoogleDataDialog_ = false;
    this.updateComplete.then(
        () => focusWithoutInk(this.$.manageOtherGoogleDataRow));
  }

  protected onSubLabelLinkClicked_(e: CustomEvent<{id: string}>) {
    // <if expr="not is_chromeos">
    if (e.detail.id === 'signOutLink') {
      this.syncBrowserProxy_.signOut(/*delete_profile=*/ false);
      this.metricsBrowserProxy_.recordAction(
          'Settings.DeleteBrowsingData.CookiesSignOutLinkClick');
      return;
    }
    // </if>
    assertNotReached(`Invalid sub-label link with id: ${e.detail.id}`);
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
