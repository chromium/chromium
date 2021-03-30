// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-import-data-dialog' is a component for importing
 * bookmarks and other data from other sources.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/settings_checkbox.js';
import '../controls/settings_toggle_button.js';
import '../icons.js';
import '../settings_vars_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';

import {BrowserProfile, ImportDataBrowserProxy, ImportDataBrowserProxyImpl, ImportDataStatus} from './import_data_browser_proxy.js';

Polymer({
  is: 'settings-import-data-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /** @private {!Array<!BrowserProfile>} */
    browserProfiles_: Array,

    /** @private {!BrowserProfile} */
    selected_: {
      type: Object,
      observer: 'updateImportDataTypesSelected_',
    },

    /**
     * Whether none of the import data categories is selected.
     * @private
     */
    noImportDataTypeSelected_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    importStatus_: {
      type: String,
      value: ImportDataStatus.INITIAL,
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    importStatusEnum_: {
      type: Object,
      value: ImportDataStatus,
    },
  },

  listeners: {
    'settings-boolean-control-change': 'updateImportDataTypesSelected_',
  },

  /** @private {?ImportDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.browserProxy_ = ImportDataBrowserProxyImpl.getInstance();
    this.browserProxy_.initializeImportDialog().then(data => {
      this.browserProfiles_ = data;
      this.selected_ = this.browserProfiles_[0];

      // Show the dialog only after the browser profiles data is populated
      // to avoid UI flicker.
      this.$.dialog.showModal();
    });

    this.addWebUIListener('import-data-status-changed', importStatus => {
      this.importStatus_ = importStatus;
      if (this.hasImportStatus_(ImportDataStatus.FAILED)) {
        this.closeDialog_();
      }
    });
  },

  /**
   * @param {string} name
   * @param {string} profileName
   * @return {string}
   * @private
   */
  getProfileDisplayName_(name, profileName) {
    return profileName ? `${name} - ${profileName}` : name;
  },

  /** @private */
  updateImportDataTypesSelected_() {
    const checkboxes = this.shadowRoot.querySelectorAll(
        'settings-checkbox[checked]:not([hidden])');
    this.noImportDataTypeSelected_ = checkboxes.length === 0;
  },

  /**
   * @param {!ImportDataStatus} status
   * @return {boolean} Whether |status| is the current status.
   * @private
   */
  hasImportStatus_(status) {
    return this.importStatus_ === status;
  },

  /** @private */
  isImportFromFileSelected_() {
    // The last entry in |browserProfiles_| always refers to dummy profile for
    // importing from a bookmarks file.
    return this.selected_.index === this.browserProfiles_.length - 1;
  },

  /**
   * @return {string}
   * @private
   */
  getActionButtonText_() {
    return this.i18n(
        this.isImportFromFileSelected_() ? 'importChooseFile' : 'importCommit');
  },

  /** @private */
  onBrowserProfileSelectionChange_() {
    this.selected_ = this.browserProfiles_[this.$.browserSelect.selectedIndex];
  },

  /** @private */
  onActionButtonTap_() {
    const checkboxes = /** @type {!NodeList<!SettingsCheckboxElement>} */ (
        this.shadowRoot.querySelectorAll('settings-checkbox'));
    if (this.isImportFromFileSelected_()) {
      this.browserProxy_.importFromBookmarksFile();
    } else {
      const types = {};
      checkboxes.forEach(checkbox => {
        types[checkbox.pref.key] = checkbox.checked && !checkbox.hidden;
      });
      this.browserProxy_.importData(this.$.browserSelect.selectedIndex, types);
    }
    checkboxes.forEach(checkbox => checkbox.sendPrefChange());
  },

  /** @private */
  closeDialog_() {
    this.$.dialog.close();
  },

  /**
   * @return {boolean} Whether the import button should be disabled.
   * @private
   */
  shouldDisableImport_() {
    return this.hasImportStatus_(ImportDataStatus.IN_PROGRESS) ||
        this.noImportDataTypeSelected_;
  },
});
