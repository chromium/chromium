// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import '//resources/js/util.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
// <if expr="not is_chromeos">
import '//resources/cr_elements/cr_toast/cr_toast.js';
// </if>

import './sync_encryption_options.js';
import '../privacy_page/personalization_options.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
// <if expr="not is_chromeos">
import './sync_account_control.js';

// </if>

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SyncBrowserProxy, SyncPrefs, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, PageStatus, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import type {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
// <if expr="is_chromeos">
import type {SettingsPersonalizationOptionsElement} from '../privacy_page/personalization_options.js';
// </if>

import type {Route} from '../router.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

// <if expr="is_chromeos">
import type {SettingsSyncEncryptionOptionsElement} from './sync_encryption_options.js';
// </if>
// clang-format on

import {getTemplate} from './sync_page.html.js';

export interface SettingsSyncPageElement {
  $: {
    encryptionCollapse: CrCollapseElement,
  };
}

/**
 * @fileoverview
 * 'settings-sync-page' is the settings page containing sync settings.
 */

const SettingsSyncPageElementBase = SettingsViewMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export class SettingsSyncPageElement extends SettingsSyncPageElementBase {
  static get is() {
    return 'settings-sync-page';
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

      focusConfig: {
        type: Object,
        observer: 'onFocusConfigChange_',
      },

      pageStatusEnum_: {
        type: Object,
        value: PageStatus,
        readOnly: true,
      },

      /**
       * The current page status. Defaults to |CONFIGURE| such that the
       * searching algorithm can search useful content when the page is not
       * visible to the user.
       */
      pageStatus_: {
        type: String,
        value: PageStatus.CONFIGURE,
      },

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: Object,

      syncStatus_: Object,

      dataEncrypted_: {
        type: Boolean,
        computed: 'computeDataEncrypted_(syncPrefs.encryptAllData)',
      },

      encryptionExpanded_: {
        type: Boolean,
        value: false,
      },

      /** If true, override |encryptionExpanded_| to be true. */
      forceEncryptionExpanded: {
        type: Boolean,
        value: false,
      },

      /**
       * The existing passphrase input field value.
       */
      existingPassphrase_: {
        type: String,
        value: '',
      },

      /*
       * Whether enter existing passphrase UI should be shown.
       */
      showExistingPassphraseBelowAccount_: {
        type: Boolean,
        value: false,
        computed: 'computeShowExistingPassphraseBelowAccount_(' +
            'syncStatus_.signedInState, syncPrefs.passphraseRequired)',
      },

      signedIn_: {
        type: Boolean,
        value: true,
        computed: 'computeSignedIn_(syncStatus_.signedInState)',
      },

      syncDisabledByAdmin_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncDisabledByAdmin_(syncStatus_.managed)',
      },

      syncSectionDisabled_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncSectionDisabled_(' +
            'syncStatus_.signedInState, syncStatus_.disabled, ' +
            'syncStatus_.hasError, syncStatus_.statusAction, ' +
            'syncPrefs.trustedVaultKeysRequired)',
      },

      // <if expr="not is_chromeos">
      showSetupCancelDialog_: {
        type: Boolean,
        value: false,
      },
      // </if>

      enterPassphraseLabel_: {
        type: String,
        computed: 'computeEnterPassphraseLabel_(syncPrefs.encryptAllData,' +
            'syncPrefs.explicitPassphraseTime)',
      },

      existingPassphraseLabel_: {
        type: String,
        computed: 'computeExistingPassphraseLabel_(syncPrefs.encryptAllData,' +
            'syncPrefs.explicitPassphraseTime)',
      },

      isEeaChoiceCountry_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isEeaChoiceCountry');
        },
      },

      personalizationCollapseExpanded_: {
        type: Boolean,
        value: false,
      },

      // Exposes ChromeSigninAccessPoint enum to HTML bindings.
      accessPointEnum_: {
        type: Object,
        value: ChromeSigninAccessPoint,
      },
    };
  }

  static get observers() {
    return [
      'expandEncryptionIfNeeded_(dataEncrypted_, forceEncryptionExpanded)',
    ];
  }

  declare prefs: {[key: string]: any};
  declare focusConfig: FocusConfig;
  declare private pageStatus_: PageStatus;
  declare syncPrefs?: SyncPrefs;
  declare private syncStatus_: SyncStatus;
  declare private dataEncrypted_: boolean;
  declare private encryptionExpanded_: boolean;
  declare forceEncryptionExpanded: boolean;
  declare private existingPassphrase_: string;
  declare private showExistingPassphraseBelowAccount_: boolean;
  declare private signedIn_: boolean;
  declare private syncDisabledByAdmin_: boolean;
  declare private syncSectionDisabled_: boolean;
  declare private isEeaChoiceCountry_: boolean;
  declare private personalizationCollapseExpanded_: boolean;

  // <if expr="not is_chromeos">
  declare private showSetupCancelDialog_: boolean;
  // </if>

  declare private enterPassphraseLabel_: TrustedHTML;
  declare private existingPassphraseLabel_: TrustedHTML;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  private collapsibleSectionsInitialized_: boolean;
  private didAbort_: boolean;
  private setupCancelConfirmed_: boolean;
  private beforeunloadCallback_: ((e: Event) => void)|null;
  private unloadCallback_: (() => void)|null;

  constructor() {
    super();

    /**
     * The beforeunload callback is used to show the 'Leave site' dialog. This
     * makes sure that the user has the chance to go back and confirm the sync
     * opt-in before leaving.
     *
     * This property is non-null if the user is currently navigated on the sync
     * settings route.
     */
    this.beforeunloadCallback_ = null;

    /**
     * The unload callback is used to cancel the sync setup when the user hits
     * the browser back button after arriving on the page.
     * Note = Cases like closing the tab or reloading don't need to be handled;
     * because they are already caught in |PeopleHandler::~PeopleHandler|
     * from the C++ code.
     */
    this.unloadCallback_ = null;

    /**
     * Whether the initial layout for collapsible sections has been computed. It
     * is computed only once; the first time the sync status is updated.
     */
    this.collapsibleSectionsInitialized_ = false;

    /**
     * Whether the user decided to abort sync.
     */
    this.didAbort_ = true;

    /**
     * Whether the user confirmed the cancellation of sync.
     */
    this.setupCancelConfirmed_ = false;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'page-status-changed', this.handlePageStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    this.syncBrowserProxy_.getSyncStatus().then(
        this.onSyncStatusChanged_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.onSyncStatusChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    const router = Router.getInstance();
    if (router.getRoutes().SYNC.contains(router.getCurrentRoute())) {
      this.onNavigateAwayFromPage_();
    }

    if (this.beforeunloadCallback_) {
      window.removeEventListener('beforeunload', this.beforeunloadCallback_);
      this.beforeunloadCallback_ = null;
    }
    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  }

  private onSyncStatusChanged_(syncStatus: SyncStatus) {
    this.syncStatus_ = syncStatus;

    // <if expr="not is_chromeos">
    if (Router.getInstance().getCurrentRoute() === routes.SYNC &&
        !this.shouldShowSyncPage_()) {
      this.onNavigateAwayFromPage_();
      Router.getInstance().navigateTo(routes.PEOPLE);
    }
    // </if>
  }

  // <if expr="is_chromeos">
  getEncryptionOptions(): SettingsSyncEncryptionOptionsElement|null {
    return this.shadowRoot!.querySelector('settings-sync-encryption-options');
  }

  getPersonalizationOptions(): SettingsPersonalizationOptionsElement|null {
    return this.shadowRoot!.querySelector('settings-personalization-options');
  }
  // </if>

  private computeSignedIn_(): boolean {
    return this.syncStatus_.signedInState === SignedInState.SYNCING;
  }

  private computeSyncSectionDisabled_(): boolean {
    return this.syncStatus_ !== undefined &&
        (this.syncStatus_.signedInState !== SignedInState.SYNCING ||
         !!this.syncStatus_.disabled ||
         (!!this.syncStatus_.hasError &&
          this.syncStatus_.statusAction !== StatusAction.ENTER_PASSPHRASE &&
          this.syncStatus_.statusAction !==
              StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS));
  }

  private computeSyncDisabledByAdmin_(): boolean {
    return this.syncStatus_ !== undefined && !!this.syncStatus_.managed;
  }

  private onFocusConfigChange_() {
    this.focusConfig.set(
        Router.getInstance().getRoutes().SYNC_ADVANCED.path, () => {
          const toFocus =
              this.shadowRoot!.querySelector<HTMLElement>('#sync-advanced-row');
          assert(toFocus);
          focusWithoutInk(toFocus);
        });
  }

  // <if expr="not is_chromeos">
  private onSetupCancelDialogBack_() {
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#setupCancelDialog')!.cancel();
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_CancelCancelAdvancedSyncSettings');
  }

  private onSetupCancelDialogConfirm_() {
    this.setupCancelConfirmed_ = true;
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#setupCancelDialog')!.close();
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().BASIC);
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_ConfirmCancelAdvancedSyncSettings');
  }

  private onSetupCancelDialogClose_() {
    this.showSetupCancelDialog_ = false;
  }

  private shouldShowSyncPage_(): boolean {
    return !loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') ||
        !this.syncStatus_ ||
        this.syncStatus_.signedInState === SignedInState.SYNCING;
  }
  // </if>

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    const router = Router.getInstance();
    if (router.getCurrentRoute() === router.getRoutes().SYNC) {
      // <if expr="not is_chromeos">
      if (!this.shouldShowSyncPage_()) {
        this.onNavigateAwayFromPage_();
        Router.getInstance().navigateTo(routes.PEOPLE);
        return;
      }
      // </if>

      this.onNavigateToPage_();
      return;
    }

    if (router.getRoutes().SYNC.contains(router.getCurrentRoute())) {
      return;
    }

    const searchParams =
        Router.getInstance().getQueryParameters().get('search');
    if (searchParams) {
      // User navigated away via searching. Cancel sync without showing
      // confirmation dialog.
      this.onNavigateAwayFromPage_();
      return;
    }

    // <if expr="not is_chromeos">
    const userActionCancelsSetup = this.syncStatus_ &&
        this.syncStatus_.firstSetupInProgress && this.didAbort_;
    if (userActionCancelsSetup && !this.setupCancelConfirmed_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_BackOnAdvancedSyncSettings');
      // Show the 'Cancel sync?' dialog.
      // Yield so that other |currentRouteChanged| observers are called,
      // before triggering another navigation (and another round of observers
      // firing). Triggering navigation from within an observer leads to some
      // undefined behavior and runtime errors.
      requestAnimationFrame(() => {
        router.navigateTo(router.getRoutes().SYNC);
        this.showSetupCancelDialog_ = true;
      });
      return;
    }

    // Reset variable.
    this.setupCancelConfirmed_ = false;

    // </if>

    this.onNavigateAwayFromPage_();
  }

  private isStatus_(expectedPageStatus: PageStatus): boolean {
    return expectedPageStatus === this.pageStatus_;
  }

  private onNavigateToPage_() {
    const router = Router.getInstance();
    assert(router.getCurrentRoute() === router.getRoutes().SYNC);
    if (this.beforeunloadCallback_) {
      return;
    }

    this.collapsibleSectionsInitialized_ = false;

    // Display loading page until the settings have been retrieved.
    this.pageStatus_ = PageStatus.SPINNER;

    this.syncBrowserProxy_.didNavigateToSyncPage();

    this.beforeunloadCallback_ = event => {
      // When the user tries to leave the sync setup, show the 'Leave site'
      // dialog.
      if (this.syncStatus_ && this.syncStatus_.firstSetupInProgress) {
        event.preventDefault();

        chrome.metricsPrivate.recordUserAction(
            'Signin_Signin_AbortAdvancedSyncSettings');
      }
    };
    window.addEventListener('beforeunload', this.beforeunloadCallback_);

    this.unloadCallback_ = this.onNavigateAwayFromPage_.bind(this);
    window.addEventListener('unload', this.unloadCallback_);
  }

  private onNavigateAwayFromPage_() {
    if (!this.beforeunloadCallback_) {
      return;
    }

    // Reset the status to CONFIGURE such that the searching algorithm can
    // search useful content when the page is not visible to the user.
    this.pageStatus_ = PageStatus.CONFIGURE;

    this.syncBrowserProxy_.didNavigateAwayFromSyncPage(this.didAbort_);

    window.removeEventListener('beforeunload', this.beforeunloadCallback_);
    this.beforeunloadCallback_ = null;

    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
    this.pageStatus_ = PageStatus.CONFIGURE;
  }

  private onActivityControlsClick_() {
    chrome.metricsPrivate.recordUserAction('Sync_OpenActivityControlsPage');
    this.syncBrowserProxy_.openActivityControlsUrl();
    window.open(loadTimeData.getString('activityControlsUrl'));
  }

  private onLinkedServicesClick_() {
    this.metricsBrowserProxy_.recordAction('Sync_OpenLinkedServicesPage');
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('linkedServicesUrl'));
  }

  private onSyncDashboardLinkClick_() {
    window.open(loadTimeData.getString('syncDashboardUrl'));
  }

  private computeDataEncrypted_(): boolean {
    return !!this.syncPrefs && this.syncPrefs.encryptAllData;
  }

  private computeEnterPassphraseLabel_(): TrustedHTML {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return window.trustedTypes!.emptyHTML;
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      // TODO(crbug.com/40765539): There's no reason why this dateless label
      // shouldn't link to 'syncErrorsHelpUrl' like the other one.
      return this.i18nAdvanced('enterPassphraseLabel');
    }

    return this.i18nAdvanced('enterPassphraseLabelWithDate', {
      tags: ['a'],
      substitutions: [
        loadTimeData.getString('syncErrorsHelpUrl'),
        this.syncPrefs.explicitPassphraseTime,
      ],
    });
  }

  private computeExistingPassphraseLabel_(): TrustedHTML {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return window.trustedTypes!.emptyHTML;
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      return this.i18nAdvanced('existingPassphraseLabel');
    }

    return this.i18nAdvanced('existingPassphraseLabelWithDate', {
      substitutions: [this.syncPrefs.explicitPassphraseTime],
    });
  }

  /**
   * Whether the encryption dropdown should be expanded by default.
   */
  private expandEncryptionIfNeeded_() {
    // Force the dropdown to expand.
    if (this.forceEncryptionExpanded) {
      this.forceEncryptionExpanded = false;
      this.encryptionExpanded_ = true;
      return;
    }

    this.encryptionExpanded_ = this.dataEncrypted_;
  }

  private onResetSyncClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events as the |cr-expand-button|
      // prevents the default which will prevent the navigation to the link.
      event.stopPropagation();
    }
  }

  /**
   * Sends the user-entered existing password to re-enable sync.
   */
  private onSubmitExistingPassphraseClick_(e: KeyboardEvent) {
    if (e.type === 'keypress' && e.key !== 'Enter') {
      return;
    }

    this.syncBrowserProxy_.setDecryptionPassphrase(this.existingPassphrase_)
        .then(
            sucessfullySet => this.handlePageStatusChanged_(
                this.computePageStatusAfterPassphraseChange_(sucessfullySet)));

    this.existingPassphrase_ = '';
  }

  private onPassphraseChanged_(e: CustomEvent<{didChange: boolean}>) {
    this.handlePageStatusChanged_(
        this.computePageStatusAfterPassphraseChange_(e.detail.didChange));
  }

  private computePageStatusAfterPassphraseChange_(successfullyChanged: boolean):
      PageStatus {
    if (!successfullyChanged) {
      return PageStatus.PASSPHRASE_FAILED;
    }

    // Stay on the setup page if the user hasn't approved sync settings yet.
    // Otherwise, close sync setup.
    return this.syncStatus_ && this.syncStatus_.firstSetupInProgress ?
        PageStatus.CONFIGURE :
        PageStatus.DONE;
  }

  /**
   * Called when the page status updates.
   */
  private handlePageStatusChanged_(pageStatus: PageStatus) {
    const router = Router.getInstance();
    switch (pageStatus) {
      case PageStatus.SPINNER:
      case PageStatus.CONFIGURE:
        this.pageStatus_ = pageStatus;
        return;
      case PageStatus.DONE:
        if (router.getCurrentRoute() === router.getRoutes().SYNC) {
          router.navigateTo(router.getRoutes().PEOPLE);
        }
        return;
      case PageStatus.PASSPHRASE_FAILED:
        if (this.pageStatus_ === PageStatus.CONFIGURE && this.syncPrefs &&
            this.syncPrefs.passphraseRequired) {
          const passphraseInput =
              this.shadowRoot!.querySelector<CrInputElement>(
                  '#existingPassphraseInput')!;
          passphraseInput.invalid = true;
          passphraseInput.focusInput();
        }
        return;
      default:
        assertNotReached();
    }
  }

  private onLearnMoreClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  }

  // <if expr="not is_chromeos">
  private shouldShowSyncAccountControl_(): boolean {
    return this.syncStatus_ !== undefined &&
        !!this.syncStatus_.syncSystemEnabled &&
        loadTimeData.getBoolean('signinAllowed');
  }
  // </if>

  private computeShowExistingPassphraseBelowAccount_(): boolean {
    return this.syncStatus_ !== undefined &&
        this.syncStatus_.signedInState === SignedInState.SYNCING &&
        this.syncPrefs !== undefined && !!this.syncPrefs.passphraseRequired;
  }

  private onSyncAdvancedClick_() {
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC_ADVANCED);
  }

  /**
   * @param e The event passed from settings-sync-account-control.
   */
  private onSyncSetupDone_(e: CustomEvent<boolean>) {
    if (e.detail) {
      this.didAbort_ = false;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_ConfirmAdvancedSyncSettings');
    } else {
      this.setupCancelConfirmed_ = true;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_CancelAdvancedSyncSettings');
    }
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().BASIC);
  }

  /**
   * Focuses the passphrase input element if it is available and the page is
   * visible.
   */
  private focusPassphraseInput_() {
    const passphraseInput = this.shadowRoot!.querySelector<CrInputElement>(
        '#existingPassphraseInput');
    const router = Router.getInstance();
    if (passphraseInput &&
        router.getCurrentRoute() === router.getRoutes().SYNC) {
      passphraseInput.focus();
    }
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-page': SettingsSyncPageElement;
  }
}

customElements.define(SettingsSyncPageElement.is, SettingsSyncPageElement);
