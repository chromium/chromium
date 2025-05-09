// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog-v2' allows the user to
 * delete browsing data that has been cached by Chromium.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../controls/settings_checkbox.js';
import '../settings_shared.css.js';
// <if expr="not is_chromeos">
import './clear_browsing_data_account_indicator.js';
// </if>
import './clear_browsing_data_time_picker.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import type {ClearBrowsingDataBrowserProxy} from './clear_browsing_data_browser_proxy.js';
import {BrowsingDataType, ClearBrowsingDataBrowserProxyImpl} from './clear_browsing_data_browser_proxy.js';
import {getTemplate} from './clear_browsing_data_dialog_v2.html.js';
import type {SettingsClearBrowsingDataTimePicker} from './clear_browsing_data_time_picker.js';

export interface SettingsClearBrowsingDataDialogV2Element {
  $: {
    cancelButton: CrButtonElement,
    clearButton: CrButtonElement,
    deleteBrowsingDataDialog: CrDialogElement,
    showMoreButton: CrButtonElement,
    manageOtherGoogleDataRow: HTMLElement,
    timePicker: SettingsClearBrowsingDataTimePicker,
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
  pref: chrome.settingsPrivate.PrefObject;
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
  }
}

const SettingsClearBrowsingDataDialogV2ElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));

export class SettingsClearBrowsingDataDialogV2Element extends
    SettingsClearBrowsingDataDialogV2ElementBase {
  static get is() {
    return 'settings-clear-browsing-data-dialog-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dataTypesExpanded_: {
        type: Boolean,
        value: false,
      },

      expandedBrowsingDataTypeOptionsList_: Array,

      moreBrowsingDataTypeOptionsList_: Array,
    };
  }

  declare private dataTypesExpanded_: boolean;
  declare private expandedBrowsingDataTypeOptionsList_:
      BrowsingDataTypeOption[];
  declare private moreBrowsingDataTypeOptionsList_: BrowsingDataTypeOption[];

  private clearBrowsingDataBrowserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUiListener(
        'browsing-data-counter-text-update',
        this.updateCounterText_.bind(this));

    this.setUpDataTypeOptionLists_();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.clearBrowsingDataBrowserProxy_.initialize();
  }

  private setUpDataTypeOptionLists_() {
    const expandedOptionsList: BrowsingDataTypeOption[] = [];
    const moreOptionsList: BrowsingDataTypeOption[] = [];

    ALL_BROWSING_DATATYPES_LIST.forEach((datatype) => {
      const datatypeOption = {
        label: getDataTypeLabel(datatype),
        pref: this.getPref(getDataTypePrefName(datatype)),
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
  private updateCounterText_(prefName: string, text: string) {
    // If the corresponding datatype is in the expanded options list, update the
    // sub-label.
    const expandedListIndex =
        this.expandedBrowsingDataTypeOptionsList_.map(option => option.pref.key)
            .indexOf(prefName);
    if (expandedListIndex !== -1) {
      this.set(
          `expandedBrowsingDataTypeOptionsList_.${expandedListIndex}.subLabel`,
          text);
      return;
    }

    // If the datatype is not found in the expanded options list, it should be
    // in the more options list.
    const moreListIndex =
        this.moreBrowsingDataTypeOptionsList_.map(option => option.pref.key)
            .indexOf(prefName);
    assert(moreListIndex !== -1);
    this.set(
        `moreBrowsingDataTypeOptionsList_.${moreListIndex}.subLabel`, text);
  }

  private shouldDataTypeBeExpanded_(datatype: BrowsingDataType) {
    return DEFAULT_BROWSING_DATATYPES_LIST.includes(datatype) ||
        this.getPref(getDataTypePrefName(datatype)).value;
  }

  private onTimePeriodChanged_() {
    this.clearBrowsingDataBrowserProxy_.restartCounters(
        /*isBasic=*/ false, this.$.timePicker.getSelectedTimePeriod());
  }

  private onCancelClick_() {
    this.$.deleteBrowsingDataDialog.close();
  }

  private onClearBrowsingDataClick_() {
    // TODO(crbug.com/397187800): Trigger the deletion and update prefs.
  }

  private onShowMoreClick_() {
    this.dataTypesExpanded_ = true;
  }

  private shouldHideShowMoreButton_() {
    return this.dataTypesExpanded_ || !this.moreBrowsingDataTypeOptionsList_ ||
        this.moreBrowsingDataTypeOptionsList_.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-dialog-v2':
        SettingsClearBrowsingDataDialogV2Element;
  }
}

customElements.define(
    SettingsClearBrowsingDataDialogV2Element.is,
    SettingsClearBrowsingDataDialogV2Element);
