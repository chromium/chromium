// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '//resources/js/assert.m.js';
import '//resources/js/util.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import {SyncBrowserProxy, SyncBrowserProxyImpl, StatusAction, SyncStatus, SyncPrefs} from './sync_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {Route, Router, RouteObserverBehavior} from '../router.js';
import '../settings_shared_css.js';


/**
 * Names of the individual data type properties to be cached from
 * SyncPrefs when the user checks 'Sync All'.
 * @type {!Array<string>}
 */
const SyncPrefsIndividualDataTypes = [
  'appsSynced',
  'extensionsSynced',
  'preferencesSynced',
  'autofillSynced',
  'typedUrlsSynced',
  'themesSynced',
  'bookmarksSynced',
  'readingListSynced',
  'passwordsSynced',
  'tabsSynced',
  'paymentsIntegrationEnabled',
  'wifiConfigurationsSynced',
];

/**
 * Names of the radio buttons which allow the user to choose their data sync
 * mechanism.
 * @enum {string}
 */
const RadioButtonNames = {
  SYNC_EVERYTHING: 'sync-everything',
  CUSTOMIZE_SYNC: 'customize-sync',
};

/**
 * @fileoverview
 * 'settings-sync-controls' contains all sync data type controls.
 */
Polymer({
  is: 'settings-sync-controls',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    hidden: {
      type: Boolean,
      value: false,
      computed: 'syncControlsHidden_(' +
          'syncStatus.signedIn, syncStatus.disabled, syncStatus.hasError)',
      reflectToAttribute: true,
    },

    /**
     * The current sync preferences, supplied by SyncBrowserProxy.
     * @type {SyncPrefs|undefined}
     */
    syncPrefs: Object,

    /**
     * The current sync status, supplied by the parent.
     * @type {SyncStatus}
     */
    syncStatus: {
      type: Object,
      observer: 'syncStatusChanged_',
    },
  },

  /** @private {?SyncBrowserProxy} */
  browserProxy_: null,

  /**
   * Caches the individually selected synced data types. This is used to
   * be able to restore the selections after checking and unchecking Sync All.
   * @private {?Object}
   */
  cachedSyncPrefs_: null,

  /** @override */
  created() {
    this.browserProxy_ = SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const router = Router.getInstance();
    if (router.getCurrentRoute() ===
        /** @type {{ SYNC_ADVANCED: !Route }} */
        (router.getRoutes()).SYNC_ADVANCED) {
      this.browserProxy_.didNavigateToSyncPage();
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.syncPrefs = syncPrefs;

    // If autofill is not registered or synced, force Payments integration off.
    if (!this.syncPrefs.autofillRegistered || !this.syncPrefs.autofillSynced) {
      this.set('syncPrefs.paymentsIntegrationEnabled', false);
    }
  },

  /**
   * @return {string} Computed binding returning the selected sync data radio
   *     button.
   * @private
   */
  selectedSyncDataRadio_: function() {
    return this.syncPrefs.syncAllDataTypes ? RadioButtonNames.SYNC_EVERYTHING :
                                             RadioButtonNames.CUSTOMIZE_SYNC;
  },

  /**
   * Called when the sync data radio button selection changes.
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onSyncDataRadioSelectionChanged_: function(event) {
    const syncAllDataTypes =
        event.detail.value === RadioButtonNames.SYNC_EVERYTHING;
    this.set('syncPrefs.syncAllDataTypes', syncAllDataTypes);
    this.handleSyncAllDataTypesChanged_(syncAllDataTypes);
  },

  /**
   * Handler for when the sync all data types checkbox is changed.
   * @param {!Event} event
   * @private
   */
  onSyncAllDataTypesChanged_(event) {
    this.handleSyncAllDataTypesChanged_(event.target.checked);
  },

  /**
   * @param {boolean} syncAllDataTypes
   * @private
   */
  handleSyncAllDataTypesChanged_: function(syncAllDataTypes) {
    if (syncAllDataTypes) {
      this.set('syncPrefs.syncAllDataTypes', true);

      // Cache the previously selected preference before checking every box.
      this.cachedSyncPrefs_ = {};
      for (const dataType of SyncPrefsIndividualDataTypes) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedSyncPrefs_[dataType] = this.syncPrefs[dataType];

        this.set(['syncPrefs', dataType], true);
      }
    } else if (this.cachedSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of SyncPrefsIndividualDataTypes) {
        this.set(['syncPrefs', dataType], this.cachedSyncPrefs_[dataType]);
      }
    }
    chrome.metricsPrivate.recordUserAction(
        syncAllDataTypes ? 'Sync_SyncEverything' : 'Sync_CustomizeSync');
    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * Handler for when any sync data type checkbox is changed (except autofill).
   * @private
   */
  onSingleSyncDataTypeChanged_() {
    assert(this.syncPrefs);
    this.browserProxy_.setSyncDatatypes(this.syncPrefs);
  },

  /**
   * Handler for when the autofill data type checkbox is changed.
   * @private
   */
  onAutofillDataTypeChanged_() {
    this.set(
        'syncPrefs.paymentsIntegrationEnabled', this.syncPrefs.autofillSynced);

    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * Handler for when the autofill data type checkbox is changed.
   * @private
   */
  onTypedUrlsDataTypeChanged_() {
    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * @param {boolean} syncAllDataTypes
   * @param {boolean} autofillSynced
   * @return {boolean} Whether the sync checkbox should be disabled.
   */
  shouldPaymentsCheckboxBeDisabled_(syncAllDataTypes, autofillSynced) {
    return syncAllDataTypes || !autofillSynced;
  },

  /** @private */
  syncStatusChanged_() {
    const router = Router.getInstance();
    const routes =
        /**
         * @type {{ SYNC: !Route,
         *           SYNC_ADVANCED: !Route }}
         */
        (router.getRoutes());
    if (router.getCurrentRoute() === routes.SYNC_ADVANCED &&
        this.syncControlsHidden_()) {
      router.navigateTo(routes.SYNC);
    }
  },

  /**
   * @return {boolean} Whether the sync controls are hidden.
   * @private
   */
  syncControlsHidden_() {
    if (!this.syncStatus) {
      // Show sync controls by default.
      return false;
    }

    if (!this.syncStatus.signedIn || this.syncStatus.disabled) {
      return true;
    }

    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !==
        StatusAction.ENTER_PASSPHRASE &&
        this.syncStatus.statusAction !==
        StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS;
  },
});
