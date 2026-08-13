// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/policy/cr_policy_indicator.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {shouldShowSyncTogglesForStatusAction, SignedInState, StatusAction, SyncBrowserProxyImpl, syncPrefsIndividualDataTypes, UserSelectableType} from '/shared/settings/people_page/sync_browser_proxy.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {getCss} from './sync_controls.css.js';
import {getHtml} from './sync_controls.html.js';

import {loadTimeData} from '../i18n_setup.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {BatchUploadPromoProxyImpl} from 'chrome://resources/js/batch_upload_promo/batch_upload_promo_proxy.js';

// clang-format on

/**
 * Names of the radio buttons which allow the user to choose their data sync
 * mechanism.
 */
enum RadioButtonNames {
  SYNC_EVERYTHING = 'sync-everything',
  CUSTOMIZE_SYNC = 'customize-sync',
}

type SyncPrefsBooleanKey = keyof Omit<SyncPrefs, 'explicitPassphraseTime'>;

/**
 * @fileoverview
 * 'settings-sync-controls' contains all sync data type controls.
 */

const SettingsSyncControlsElementBase =
    RouteObserverMixinLit(WebUiListenerMixinLit(CrLitElement));

export type SyncControlsElement = SettingsSyncControlsElement;

export class SettingsSyncControlsElement extends
    SettingsSyncControlsElementBase {
  static get is() {
    return 'settings-sync-controls';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hidden: {
        type: Boolean,
        reflect: true,
      },

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: {type: Object},

      /**
       * The current sync status, supplied by the parent.
       */
      syncStatus: {type: Object},

      /**
       * Communicates to the user that the toggles are disabled because sync is
       * disabled by their administrator.
       */
      showSyncDisabledInformation: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Returns whether this element is currently displayed on the account
       * settings page. True when `replaceSyncPromosWithSignInPromos` is enabled
       * and the user navigates to the account page.
       */
      isAccountSettingsPage_: {type: Boolean},

      batchUploadPromoHTML_: {type: String},
    };
  }

  override accessor hidden: boolean = false;
  accessor syncPrefs: SyncPrefs|undefined;
  accessor syncStatus: SyncStatus|null = null;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  /**
   * Caches the individually selected synced data types. This is used to
   * be able to restore the selections after checking and unchecking Sync All.
   */
  private cachedSyncPrefs_: Partial<SyncPrefs>|null = null;
  accessor showSyncDisabledInformation: boolean = false;
  protected accessor isAccountSettingsPage_: boolean = false;
  protected accessor batchUploadPromoHTML_: TrustedHTML =
      window.trustedTypes!.emptyHTML;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const showBatchUploadPromo = loadTimeData.valueExists('unoPhase2FollowUp') ?
        loadTimeData.getBoolean('unoPhase2FollowUp') :
        loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos');

    if (showBatchUploadPromo) {
      BatchUploadPromoProxyImpl.getInstance()
          .callbackRouter.onLocalDataCountChanged.addListener(
              (localDataCount: number) => {
                this.batchUploadPromoLocalDataCountChanged_(localDataCount);
              });
      BatchUploadPromoProxyImpl.getInstance()
          .handler.getBatchUploadPromoLocalDataCount()
          .then(({localDataCount}) => {
            this.batchUploadPromoLocalDataCountChanged_(localDataCount);
          });
    }

    const router = Router.getInstance();
    const currentRoute = router.getCurrentRoute();
    if (currentRoute === routes.SYNC_ADVANCED) {
      this.syncBrowserProxy_.didNavigateToSyncPage();
    }
    if (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') &&
        currentRoute === routes.ACCOUNT) {
      this.isAccountSettingsPage_ = true;
      this.syncBrowserProxy_.didNavigateToAccountSettingsPage();
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('syncStatus') ||
        changedProperties.has('syncPrefs') ||
        changedPrivateProperties.has('isAccountSettingsPage_')) {
      this.hidden = this.syncControlsHidden_();
      this.showSyncDisabledInformation =
          this.computeShowSyncDisabledInformation_();
    }

    if (changedProperties.has('syncStatus')) {
      this.syncStatusChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('batchUploadPromoHTML_')) {
      this.attachOpenBatchUploadLinkClick_();
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
  }

  private async batchUploadPromoLocalDataCountChanged_(localDataCount: number):
      Promise<void> {
    const showBatchUploadPromo = loadTimeData.valueExists('unoPhase2FollowUp') ?
        loadTimeData.getBoolean('unoPhase2FollowUp') :
        loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos');

    if (!showBatchUploadPromo) {
      return;
    }

    if (localDataCount === 0) {
      this.batchUploadPromoHTML_ = window.trustedTypes!.emptyHTML;
      return;
    }

    const batchUploadPromoString =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'batchUploadPromoLabel', localDataCount);

    // We need the HTML representation instead of the string since the string
    // holds a link.
    this.batchUploadPromoHTML_ =
        sanitizeInnerHtml(batchUploadPromoString, {tags: ['a'], attrs: ['id']});
  }

  protected shouldShowBatchUploadPromo_(): boolean {
    const showBatchUploadPromo = loadTimeData.valueExists('unoPhase2FollowUp') ?
        loadTimeData.getBoolean('unoPhase2FollowUp') :
        loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos');

    if (!showBatchUploadPromo) {
      return false;
    }

    if (!this.isAccountSettingsPage_) {
      return false;
    }

    return this.batchUploadPromoHTML_ !== window.trustedTypes!.emptyHTML;
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
    BatchUploadPromoProxyImpl.getInstance().handler.onBatchUploadPromoClicked();
  }

  /**
   * @return Computed binding returning the selected sync data radio button.
   */
  protected selectedSyncDataRadio_(): string {
    return this.syncPrefs?.syncAllDataTypes ? RadioButtonNames.SYNC_EVERYTHING :
                                              RadioButtonNames.CUSTOMIZE_SYNC;
  }

  /**
   * Called when the sync data radio button selection changes.
   */
  protected onSyncDataRadioSelectedChanged_(
      event: CustomEvent<{value: string}>) {
    const syncAllDataTypes =
        event.detail.value === RadioButtonNames.SYNC_EVERYTHING;
    const previous = !!this.syncPrefs?.syncAllDataTypes;
    if (previous !== syncAllDataTypes) {
      this.handleSyncAllDataTypesChanged_(syncAllDataTypes);
    }
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    if (!loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos')) {
      return;
    }

    this.isAccountSettingsPage_ = newRoute === routes.ACCOUNT;

    if (this.isAccountSettingsPage_ && oldRoute !== routes.ACCOUNT) {
      this.syncBrowserProxy_.didNavigateToAccountSettingsPage();
    }
  }

  protected mergedHistoryTabsToggleDisabled_(): boolean {
    return !this.syncStatus || this.syncStatus.disabled || !this.syncPrefs ||
        (this.syncPrefs.tabsManaged && this.syncPrefs.typedUrlsManaged);
  }

  protected mergedHistoryTabsTogglePolicyIndicatorShown_(): boolean {
    return !!this.syncStatus && !this.syncStatus.disabled && !!this.syncPrefs &&
        this.syncPrefs.tabsManaged && this.syncPrefs.typedUrlsManaged;
  }

  protected mergedHistoryTabsToggleChecked_(): boolean {
    return !!this.syncPrefs &&
        (this.syncPrefs.typedUrlsSynced || this.syncPrefs.tabsSynced ||
         this.syncPrefs.savedTabGroupsSynced);
  }

  protected onMergedHistoryTabsToggleChange_(event: Event) {
    assert(this.isAccountSettingsPage_);

    const toggle = event.target as CrToggleElement;

    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.HISTORY, toggle.checked);
    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.TABS, toggle.checked);
    this.syncBrowserProxy_.setSyncDatatype(
        UserSelectableType.SAVED_TAB_GROUPS, toggle.checked);
  }

  private handleSyncAllDataTypesChanged_(syncAllDataTypes: boolean) {
    assert(this.syncPrefs);
    const updatedSyncPrefs = {...this.syncPrefs};
    updatedSyncPrefs.syncAllDataTypes = syncAllDataTypes;
    if (syncAllDataTypes) {
      // Cache the previously selected preference before checking every box.
      this.cachedSyncPrefs_ = {};
      for (const dataType of syncPrefsIndividualDataTypes as
           SyncPrefsBooleanKey[]) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedSyncPrefs_[dataType] = this.syncPrefs[dataType];
        updatedSyncPrefs[dataType] = true;
      }
    } else if (this.cachedSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of syncPrefsIndividualDataTypes as
           SyncPrefsBooleanKey[]) {
        const cached = this.cachedSyncPrefs_[dataType];
        if (cached !== undefined) {
          updatedSyncPrefs[dataType] = cached;
        }
      }
    }
    this.syncPrefs = updatedSyncPrefs;
    chrome.metricsPrivate.recordUserAction(
        syncAllDataTypes ? 'Sync_SyncEverything' : 'Sync_CustomizeSync');
    this.onSingleSyncDataTypeChange_();
  }

  /**
   * Handler for when any sync data type checkbox is changed.
   */
  protected onSingleSyncDataTypeChange_(event?: Event) {
    if (this.isAccountSettingsPage_) {
      assert(event);

      const toggle = event.target as CrToggleElement;
      const type = Number(toggle.dataset['type']);
      assert(!isNaN(type));

      this.syncBrowserProxy_.setSyncDatatype(type, toggle.checked);
      return;
    }

    assert(this.syncPrefs);
    if (event) {
      const toggle = event.target as CrToggleElement;
      const pref = toggle.dataset['pref'] as SyncPrefsBooleanKey | undefined;
      if (pref) {
        this.syncPrefs[pref] = toggle.checked;
      }
    }
    this.syncBrowserProxy_.setSyncDatatypes(this.syncPrefs);
  }

  protected disableTypeCheckBox_(dataTypeManaged: boolean|undefined|null):
      boolean {
    if (!this.syncStatus) {
      return true;
    }

    if (dataTypeManaged) {
      return true;
    }

    if (this.syncStatus.signedInState === SignedInState.SYNCING) {
      return !!this.syncPrefs?.syncAllDataTypes;
    }

    // Toggles should be disabled on the account settings page if sync is
    // disabled, or if the sync prefs are undefined, which is the case e.g.
    // right after startup.
    return this.syncStatus.disabled || !this.syncPrefs;
  }

  protected showPolicyIndicator_(dataTypeManaged: boolean|undefined|null):
      boolean {
    // Do not show the indicator on the account settings page if sync is
    // disabled, as this would make the UI look too crowded and the toggles are
    // already deactivated. In the sync settings page, the toggles are hidden if
    // sync is disabled (see `syncControlsHidden_()`), so we do not need to
    // specify whether we show the indicator or not.
    if (this.isAccountSettingsPage_) {
      return !!this.syncStatus && !this.syncStatus.disabled &&
          !!dataTypeManaged;
    }

    return !!dataTypeManaged;
  }

  private computeShowSyncDisabledInformation_(): boolean {
    return this.isAccountSettingsPage_ && !!this.syncStatus?.disabled;
  }

  // <if expr="is_chromeos">
  protected hideCookieItem_(): boolean {
    return !this.syncStatus?.syncCookiesSupported ||
        (!!this.syncPrefs && !this.syncPrefs.cookiesRegistered);
  }
  // </if>

  private syncStatusChanged_() {
    const router = Router.getInstance();
    if (router.getCurrentRoute() === routes.SYNC_ADVANCED &&
        this.syncControlsHidden_()) {
      // Try to navigate the user to the account page, where they can find the
      // toggles. If the page does not exist, they will be redirected to the
      // people settings page from there.
      if (loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos')) {
        router.navigateTo(routes.ACCOUNT);
        return;
      }

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
    // state here. However, the controls should be hidden if there is a generic
    // sync error (e.g. a passphrase is required), or if the user has local sync
    // enabled.
    if (this.isAccountSettingsPage_) {
      return (!!this.syncStatus.hasError &&
              this.syncStatus.statusAction !== StatusAction.UPGRADE_CLIENT &&
              this.syncStatus.statusAction !==
                  StatusAction.SHOW_BOOKMARKS_LIMIT_HELP_ARTICLE) ||
          (!!this.syncPrefs && this.syncPrefs.localSyncEnabled);
    }

    if (this.syncStatus.signedInState !== SignedInState.SYNCING ||
        this.syncStatus.disabled) {
      return true;
    }

    return !!this.syncStatus.hasError &&
        !shouldShowSyncTogglesForStatusAction(this.syncStatus.statusAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-controls': SettingsSyncControlsElement;
  }
}

customElements.define(
    SettingsSyncControlsElement.is, SettingsSyncControlsElement);
