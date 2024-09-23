// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, StatusAction, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes} from '/shared/settings/people_page/sync_browser_proxy.js';
// <if expr="chromeos_lacros">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

// </if>

import {loadTimeData} from '../i18n_setup.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';

import {getTemplate} from './sync_controls.html.js';

/**
 * Names of the radio buttons which allow the user to choose their data sync
 * mechanism.
 */
enum RadioButtonNames {
  SYNC_EVERYTHING = 'sync-everything',
  CUSTOMIZE_SYNC = 'customize-sync',
}

/**
 * @fileoverview
 * 'settings-sync-controls' contains all sync data type controls.
 */

const SettingsSyncControlsElementBase = WebUiListenerMixin(PolymerElement);

export class SettingsSyncControlsElement extends
    SettingsSyncControlsElementBase {
  static get is() {
    return 'settings-sync-controls';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: false,
        computed: 'syncControlsHidden_(' +
            'syncStatus.signedIn, syncStatus.disabled, syncStatus.hasError)',
        reflectToAttribute: true,
      },

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: Object,

      /**
       * The current sync status, supplied by the parent.
       */
      syncStatus: {
        type: Object,
        observer: 'syncStatusChanged_',
      },

      // <if expr="chromeos_lacros">
      /**
       * Whether to show the new UI for OS Sync Settings and
       * Browser Sync Settings which include sublabel and
       * Apps toggle shared between Ash and Lacros.
       */
      showSyncSettingsRevamp_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showSyncSettingsRevamp'),
      },
      //</if>
    };
  }

  override hidden: boolean;
  syncPrefs?: SyncPrefs;
  syncStatus: SyncStatus;
  private showSyncSettingsRevamp_: boolean;
  private browserProxy_: SyncBrowserProxy = SyncBrowserProxyImpl.getInstance();
  private cachedSyncPrefs_: {[key: string]: any}|null;

  constructor() {
    super();

    /**
     * Caches the individually selected synced data types. This is used to
     * be able to restore the selections after checking and unchecking Sync All.
     */
    this.cachedSyncPrefs_ = null;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const router = Router.getInstance();
    if (router.getCurrentRoute() ===
        (router.getRoutes() as {SYNC_ADVANCED: Route}).SYNC_ADVANCED) {
      this.browserProxy_.didNavigateToSyncPage();
    }
  }

  // <if expr="chromeos_lacros">
  private onOsSyncSettingsLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSyncSettingsUrl'));
  }
  // </if>

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
  }

  /**
   * @return Computed binding returning the selected sync data radio button.
   */
  private selectedSyncDataRadio_(): string {
    return this.syncPrefs!.syncAllDataTypes ? RadioButtonNames.SYNC_EVERYTHING :
                                              RadioButtonNames.CUSTOMIZE_SYNC;
  }

  /**
   * Called when the sync data radio button selection changes.
   */
  private onSyncDataRadioSelectionChanged_(event:
                                               CustomEvent<{value: string}>) {
    const syncAllDataTypes =
        event.detail.value === RadioButtonNames.SYNC_EVERYTHING;
    const previous = this.syncPrefs!.syncAllDataTypes;
    if (previous !== syncAllDataTypes) {
      this.set('syncPrefs.syncAllDataTypes', syncAllDataTypes);
      this.handleSyncAllDataTypesChanged_(syncAllDataTypes);
    }
  }

  // <if expr="chromeos_lacros">
  private disableAppsToggle_(
      syncAllDataTypes: boolean, showSyncSettingsRevamp: boolean,
      appsManaged: boolean): boolean {
    return syncAllDataTypes || showSyncSettingsRevamp || appsManaged;
  }

  private showAppsPolicyIndicator_(
      appsManaged: boolean, showSyncSettingsRevamp: boolean): boolean {
    return appsManaged && !showSyncSettingsRevamp;
  }
  // </if>

  private handleSyncAllDataTypesChanged_(syncAllDataTypes: boolean) {
    if (syncAllDataTypes) {
      this.set('syncPrefs.syncAllDataTypes', true);

      // Cache the previously selected preference before checking every box.
      this.cachedSyncPrefs_ = {};
      for (const dataType of syncPrefsIndividualDataTypes) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedSyncPrefs_[dataType] =
            (this.syncPrefs as {[key: string]: any})[dataType];

        this.set(['syncPrefs', dataType], true);
      }
    } else if (this.cachedSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of syncPrefsIndividualDataTypes) {
        this.set(['syncPrefs', dataType], this.cachedSyncPrefs_[dataType]);
      }
    }
    chrome.metricsPrivate.recordUserAction(
        syncAllDataTypes ? 'Sync_SyncEverything' : 'Sync_CustomizeSync');
    this.onSingleSyncDataTypeChanged_();
  }

  /**
   * Handler for when any sync data type checkbox is changed (except autofill).
   */
  private onSingleSyncDataTypeChanged_() {
    assert(this.syncPrefs);
    this.browserProxy_.setSyncDatatypes(this.syncPrefs!);
  }

  private disableTypeCheckBox_(
      syncAllDataTypes: boolean, dataTypeManaged: boolean): boolean {
    return syncAllDataTypes || dataTypeManaged;
  }

  // <if expr="chromeos_ash">
  private hideCookieItem_(
      syncCookiesSupported: boolean, cookiesRegistered: boolean): boolean {
    return !syncCookiesSupported || !cookiesRegistered;
  }
  // </if>

  private syncStatusChanged_() {
    const router = Router.getInstance();
    const routes = router.getRoutes() as {SYNC: Route, SYNC_ADVANCED: Route};
    if (router.getCurrentRoute() === routes.SYNC_ADVANCED &&
        this.syncControlsHidden_()) {
      router.navigateTo(routes.SYNC);
    }
  }

  /**
   * @return Whether the sync controls are hidden.
   */
  private syncControlsHidden_(): boolean {
    if (!this.syncStatus) {
      // Show sync controls by default.
      return false;
    }

    if (this.syncStatus.signedInState !== SignedInState.SYNCING ||
        this.syncStatus.disabled) {
      return true;
    }

    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== StatusAction.ENTER_PASSPHRASE &&
        this.syncStatus.statusAction !==
        StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-controls': SettingsSyncControlsElement;
  }
}

customElements.define(
    SettingsSyncControlsElement.is, SettingsSyncControlsElement);
