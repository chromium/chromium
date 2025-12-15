// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, StatusAction, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes, UserSelectableType} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {getTemplate} from './sync_controls.html.js';

// <if expr="not is_chromeos">
import {loadTimeData} from '../i18n_setup.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {BatchUploadPromoProxy} from 'chrome://resources/js/batch_upload_promo/batch_upload_promo_proxy.js';
// </if>

// clang-format on

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

// <if expr="not is_chromeos">
const SettingsSyncControlsElementBase =
    RouteObserverMixin(WebUiListenerMixin(PolymerElement));
// </if>
// <if expr="is_chromeos">
const SettingsSyncControlsElementBase = WebUiListenerMixin(PolymerElement);
// </if>

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
            'syncStatus.signedIn, syncStatus.disabled, ' +
            'syncStatus.hasError, isAccountSettingsPage_, ' +
            'syncPrefs.localSyncEnabled)',
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

      /** Expose UserSelectableType enum to HTML bindings. */
      UserSelectableTypeEnum_: {
        type: Object,
        value: UserSelectableType,
      },

      /**
       * Communicates to the user that the toggles are disabled because sync is
       * disabled by their administrator.
       */
      showSyncDisabledInformation: {
        type: Boolean,
        value: false,
        computed: 'computeShowSyncDisabledInformation_(syncStatus.disabled, ' +
            'isAccountSettingsPage_)',
        reflectToAttribute: true,
      },

      /**
       * Returns whether this element is currently displayed on the account
       * settings page. Always false on ChromeOS and with
       * `replaceSyncPromosWithSignInPromos` disabled.
       */
      isAccountSettingsPage_: {
        type: Boolean,
        value: false,
      },

      // <if expr="not is_chromeos">
      batchUploadPromoLocalDataCount_: {
        type: Number,
        value: 0,
        observer: 'batchUploadPromoLocalDataCountChanged_',
      },

      batchUploadPromoString_: {
        type: String,
        value: '',
        observer: 'attachOpenBatchUploadLinkClick_',
      },
      // </if>
    };
  }

  declare hidden: boolean;
  declare syncPrefs?: SyncPrefs;
  declare syncStatus: SyncStatus|null;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private cachedSyncPrefs_: {[key: string]: any}|null;
  declare showSyncDisabledInformation: boolean;
  declare private isAccountSettingsPage_: boolean;
  // <if expr="not is_chromeos">
  declare private batchUploadPromoLocalDataCount_: number;
  declare private batchUploadPromoString_: string;
  // </if>

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

    // <if expr="not is_chromeos">
    if (loadTimeData.getBoolean('unoPhase2FollowUp')) {
      BatchUploadPromoProxy.getInstance()
          .callbackRouter.onLocalDataCountChanged.addListener(
              (batchUploadPromoData: number) => {
                this.batchUploadPromoLocalDataCount_ = batchUploadPromoData;
              });
      BatchUploadPromoProxy.getInstance()
          .handler.getBatchUploadPromoLocalDataCount()
          .then(({localDataCount}) => {
            this.batchUploadPromoLocalDataCount_ = localDataCount;
          });
    }
    // </if>

    const router = Router.getInstance();
    const currentRoute = router.getCurrentRoute();
    if (currentRoute === routes.SYNC_ADVANCED) {
      this.syncBrowserProxy_.didNavigateToSyncPage();
    }
    // <if expr="not is_chromeos">
    if (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') &&
        currentRoute === routes.ACCOUNT) {
      this.isAccountSettingsPage_ = true;
      this.syncBrowserProxy_.didNavigateToAccountSettingsPage();
    }
    // </if>
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
  }

  // <if expr="not is_chromeos">
  private async batchUploadPromoLocalDataCountChanged_(): Promise<void> {
    if (!loadTimeData.getBoolean('unoPhase2FollowUp')) {
      return;
    }

    if (!this.batchUploadPromoLocalDataCount_) {
      this.batchUploadPromoString_ = '';
      return;
    }

    this.batchUploadPromoString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'batchUploadPromoLabel', this.batchUploadPromoLocalDataCount_);
  }

  private shouldShowBatchUploadPromo_(): boolean {
    if (!loadTimeData.getBoolean('unoPhase2FollowUp')) {
      return false;
    }

    if (!this.isAccountSettingsPage_) {
      return false;
    }

    return this.batchUploadPromoLocalDataCount_ !== 0;
  }

  /**
   * Returns the HTML representation of the subtitle string. We need the HTML
   * representation instead of the string since the string holds a link.
   */
  protected getSubtitleString_(): TrustedHTML {
    return sanitizeInnerHtml(
        this.batchUploadPromoString_, {tags: ['a'], attrs: ['id']});
  }

  /** Attach the click action and aria label to the batch upload promo link. */
  private attachOpenBatchUploadLinkClick_(): void {
    const element: HTMLElement|null|undefined =
        this.shadowRoot?.querySelector(`#openBatchUploadLink`);
    if (element !== null && element !== undefined) {
      element.addEventListener('click', (me: MouseEvent) => {
        this.onPromoClicked_(me);
      });

      // Since there is a link for the batch upload, we can also be sure that
      // the containing element exists.
      const batchUploadElement: HTMLElement|null|undefined =
          this.shadowRoot?.querySelector(`#batchUploadPromo`);
      element.setAttribute('aria-label', batchUploadElement!.textContent);
    }
  }

  private onPromoClicked_(event: Event): void {
    assert(this.shouldShowBatchUploadPromo_());

    // Prevent navigation to href='#' and open the batch upload dialog instead.
    event.preventDefault();
    BatchUploadPromoProxy.getInstance().handler.onBatchUploadPromoClicked();
  }
  // </if>

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

  // <if expr="not is_chromeos">
  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    if (!loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos')) {
      return;
    }

    this.isAccountSettingsPage_ = newRoute === routes.ACCOUNT;

    if (this.isAccountSettingsPage_ && oldRoute !== routes.ACCOUNT) {
      this.syncBrowserProxy_.didNavigateToAccountSettingsPage();
    }
  }

  private mergedHistoryTabsToggleDisabled_(
      syncStatus: SyncStatus, tabsManaged: boolean,
      historyManaged: boolean): boolean {
    return !syncStatus || syncStatus.disabled || !this.syncPrefs ||
        (tabsManaged && historyManaged);
  }

  private mergedHistoryTabsTogglePolicyIndicatorShown_(
      syncStatus: SyncStatus, tabsManaged: boolean,
      historyManaged: boolean): boolean {
    return !!syncStatus && !syncStatus.disabled && tabsManaged &&
        historyManaged;
  }

  private mergedHistoryTabsToggleChecked_(syncPrefs: SyncPrefs): boolean {
    return syncPrefs.typedUrlsSynced || syncPrefs.tabsSynced ||
        syncPrefs.savedTabGroupsSynced;
  }

  private onMergedHistoryTabsToggleChanged_(event: Event) {
    assert(this.isAccountSettingsPage_);

    const toggle = event.target as CrToggleElement;

    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.HISTORY, toggle.checked);
    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.TABS, toggle.checked);
    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.SAVED_TAB_GROUPS, toggle.checked);
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
   * Handler for when any sync data type checkbox is changed.
   */
  private onSingleSyncDataTypeChanged_(_event?: Event) {
    // <if expr="not is_chromeos">
    if (this.isAccountSettingsPage_) {
      assert(_event);

      const toggle = _event?.target as CrToggleElement;
      const type = Number(toggle.dataset['type']!);
      assert(!isNaN(type));

      this.syncBrowserProxy_.setSyncDatatype(type, toggle.checked);
      return;
    }
    // </if>

    assert(this.syncPrefs);
    this.syncBrowserProxy_.setSyncDatatypes(this.syncPrefs);
  }

  private disableTypeCheckBox_(
      syncStatus: SyncStatus, syncAllDataTypes: boolean,
      dataTypeManaged: boolean): boolean {
    if (!syncStatus) {
      return true;
    }

    if (dataTypeManaged) {
      return true;
    }

    if (syncStatus.signedInState === SignedInState.SYNCING) {
      return syncAllDataTypes;
    }

    // Toggles should be disabled on the account settings page if sync is
    // disabled, or if the sync prefs are undefined, which is the case e.g.
    // right after startup.
    return syncStatus.disabled || !this.syncPrefs;
  }

  private showPolicyIndicator_(
      syncStatus: SyncStatus, dataTypeManaged: boolean): boolean {
    // Do not show the indicator on the account settings page if sync is
    // disabled, as this would make the UI look too crowded and the toggles are
    // already deactivated. In the sync settings page, the toggles are hidden if
    // sync is disabled (see `syncControlsHidden_()`), so we do not need to
    // specify whether we show the indicator or not.
    if (this.isAccountSettingsPage_) {
      return !!syncStatus && !syncStatus.disabled && dataTypeManaged;
    }

    return dataTypeManaged;
  }

  private computeShowSyncDisabledInformation_(syncDisabled: boolean): boolean {
    return this.isAccountSettingsPage_ && syncDisabled;
  }

  // <if expr="is_chromeos">
  private hideCookieItem_(
      syncCookiesSupported: boolean, cookiesRegistered: boolean): boolean {
    return !syncCookiesSupported || !cookiesRegistered;
  }
  // </if>

  private syncStatusChanged_() {
    const router = Router.getInstance();
    if (router.getCurrentRoute() === routes.SYNC_ADVANCED &&
        this.syncControlsHidden_()) {
      // <if expr="not is_chromeos">
      // Try to navigate the user to the account page, where they can find the
      // toggles. If the page does not exist, they will be redirected to the
      // people settings page from there.
      if (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos')) {
        router.navigateTo(routes.ACCOUNT);
        return;
      }
      // </if>

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

    // The account page is not shown when the user is not signed in or if they
    // are in sign in pending state, so we don't need to check for the signed in
    // state here. However, the controls should be hidden if there is a
    // passphrase error or the user has local sync enabled.
    // <if expr="not is_chromeos">
    if (this.isAccountSettingsPage_) {
      return !!this.syncStatus.hasError ||
          this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE ||
          (!!this.syncPrefs && this.syncPrefs.localSyncEnabled);
    }
    // </if>

    if (this.syncStatus.signedInState !== SignedInState.SYNCING ||
        this.syncStatus.disabled) {
      return true;
    }

    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== StatusAction.ENTER_PASSPHRASE &&
        this.syncStatus.statusAction !==
        StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS &&
        this.syncStatus.statusAction !== StatusAction.CONFIRM_SYNC_SETTINGS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-controls': SettingsSyncControlsElement;
  }
}

customElements.define(
    SettingsSyncControlsElement.is, SettingsSyncControlsElement);
