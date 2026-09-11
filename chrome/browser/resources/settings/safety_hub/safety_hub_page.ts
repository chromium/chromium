// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

// <if expr="not is_chromeos">
import '../relaunch_confirmation_dialog.js';
// </if>
import '../settings_page/settings_subpage.js';
import './safety_hub_card.js';
import './safety_hub_module.js';
import './extensions_module.js';
import './notification_permissions_module.js';
import './unused_site_permissions_module.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/passwords/password_manager_proxy.js';
import type {MetricsBrowserProxy, SafetyHubCardState} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubModuleType, SafetyHubSurfaces} from '../metrics_browser_proxy.js';
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import type {Route} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import type {CardInfo, NotificationPermission, SafetyHubBrowserProxy, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {CardState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from './safety_hub_browser_proxy.js';
import type {SettingsSafetyHubCardElement} from './safety_hub_card.js';
import type {SiteInfo} from './safety_hub_module.js';
import {getCss} from './safety_hub_page.css.js';
import {getHtml} from './safety_hub_page.html.js';

export interface SettingsSafetyHubPageElement {
  $: {
    passwords: SettingsSafetyHubCardElement,
    safeBrowsing: SettingsSafetyHubCardElement,
    version: SettingsSafetyHubCardElement,
  };
}

const SettingsSafetyHubPageElementBase =
    SettingsViewMixinLit(RelaunchMixinLit(PrefServiceObserverMixinLit(
        WebUiListenerMixinLit(I18nMixinLit(CrLitElement)))));

export class SettingsSafetyHubPageElement extends
    SettingsSafetyHubPageElementBase {
  static get is() {
    return 'settings-safety-hub-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // The object that holds data of Password Check card.
      passwordCardData_: {type: Object},

      // The object that holds data of Version Check card.
      versionCardData_: {type: Object},

      // The object that holds data of Safe Browsing card.
      safeBrowsingCardData_: {type: Object},

      // Whether Notification Permissions module should be visible.
      showNotificationPermissions_: {type: Boolean},

      // Whether Unused Site Permissions module should be visible.
      showUnusedSitePermissions_: {type: Boolean},

      // Whether Extensions module should be visible.
      showExtensions_: {type: Boolean},

      userEducationItemList_: {type: Array},

      // Whether the data for notification permissions is ready.
      hasDataForNotificationPermissions_: {type: Boolean},

      // Whether the data for unused site permissions is ready.
      hasDataForUnusedPermissions_: {type: Boolean},

      // Whether the data for extensions is ready.
      hasDataForExtensions_: {type: Boolean},

      safeBrowsingPref_: {type: Object},
    };
  }

  protected accessor passwordCardData_: CardInfo = {
    header: '',
    subheader: '',
    state: CardState.INFO,
  };
  protected accessor versionCardData_: CardInfo = {
    header: '',
    subheader: '',
    state: CardState.INFO,
  };
  protected accessor safeBrowsingCardData_: CardInfo = {
    header: '',
    subheader: '',
    state: CardState.INFO,
  };
  protected accessor showNotificationPermissions_: boolean = false;
  private accessor hasDataForNotificationPermissions_: boolean = false;
  protected accessor showUnusedSitePermissions_: boolean = false;
  private accessor hasDataForUnusedPermissions_: boolean = false;
  protected accessor showExtensions_: boolean = false;
  private accessor hasDataForExtensions_: boolean = false;
  private shouldRecordMetric_: boolean = false;
  protected accessor userEducationItemList_: SiteInfo[] = [];
  private accessor safeBrowsingPref_: chrome.settingsPrivate.PrefObject|
      undefined = undefined;
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    this.initializeCards_();
    this.initializeModules_();
    this.initializeUserEducation_();

    super.connectedCallback();

    this.mirrorPrefs({
      'generated.safe_browsing': 'safeBrowsingPref_',
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('passwordCardData_') ||
        changedPrivateProperties.has('versionCardData_') ||
        changedPrivateProperties.has('safeBrowsingCardData_') ||
        changedPrivateProperties.has('hasDataForUnusedPermissions_') ||
        changedPrivateProperties.has('hasDataForNotificationPermissions_') ||
        changedPrivateProperties.has('hasDataForExtensions_')) {
      this.onAllModulesLoaded_();
    }

    if (changedPrivateProperties.has('safeBrowsingPref_')) {
      const oldPref = changedPrivateProperties.get('safeBrowsingPref_') as
              chrome.settingsPrivate.PrefObject |
          undefined;
      if (oldPref?.value !== this.safeBrowsingPref_?.value) {
        this.onSafeBrowsingPrefChanged_();
      }
    }
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (Router.getInstance().getCurrentRoute() !== routes.SAFETY_HUB) {
      return;
    }
    // When the user navigates to the Safety Hub page, any active menu
    // notification is dismissed.
    this.browserProxy_.dismissActiveMenuNotification();

    // Record a visit to Safety Hub page if the user is still on the SH page
    // after 20 seconds.
    setTimeout(() => {
      if (Router.getInstance().getCurrentRoute() === routes.SAFETY_HUB) {
        this.browserProxy_.recordSafetyHubPageVisit();
      }
    }, 20000);

    this.metricsBrowserProxy_.recordSafetyHubImpression(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);
    this.metricsBrowserProxy_.recordSafetyHubInteraction(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);

    // Only record the metrics when the user navigates to the Safety Hub page.
    this.shouldRecordMetric_ = true;
    this.onAllModulesLoaded_();
  }

  private initializeCards_() {
    // TODO(crbug.com/40267370): Add listeners for Password and Version cards.
    this.browserProxy_.getPasswordCardData().then((data: CardInfo) => {
      this.passwordCardData_ = data;
    });

    this.browserProxy_.getSafeBrowsingCardData().then((data: CardInfo) => {
      this.safeBrowsingCardData_ = data;
    });

    this.browserProxy_.getVersionCardData().then((data: CardInfo) => {
      this.versionCardData_ = data;
    });
  }

  private initializeModules_() {
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onNotificationPermissionListChanged_(sites));

    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.addWebUiListener(
        SafetyHubEvent.EXTENSIONS_CHANGED,
        (num: number) => this.onExtensionsChanged_(num));

    this.browserProxy_.getNotificationPermissionReview().then(
        (sites: NotificationPermission[]) =>
            this.onNotificationPermissionListChanged_(sites));

    this.browserProxy_.getRevokedUnusedSitePermissionsList().then(
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.browserProxy_.getNumberOfExtensionsThatNeedReview().then(
        (num: number) => this.onExtensionsChanged_(num));
  }

  private initializeUserEducation_() {
    this.userEducationItemList_ = [
      {
        origin: this.i18n('safetyHubUserEduDataHeader'),
        detail: this.i18nAdvanced('safetyHubUserEduDataSubheader'),
        icon: 'settings20:chrome-product',
      },
      {
        origin: this.i18n('safetyHubUserEduIncognitoHeader'),
        detail: this.i18nAdvanced('safetyHubUserEduIncognitoSubheader'),
        icon: 'settings20:incognito',
      },
      {
        origin: this.i18n('safetyHubUserEduSafeBrowsingHeader'),
        detail: this.i18nAdvanced('safetyHubUserEduSafeBrowsingSubheader'),
        icon: 'cr:security',
      },
    ];
  }

  protected onPasswordsClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.PasswordsCard.StatusOnClick',
        this.passwordCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.CHECKUP);
  }

  protected onPasswordsKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onPasswordsClick_();
    }
  }

  protected onVersionClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.VersionCard.StatusOnClick',
        this.versionCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    if (this.versionCardData_.state === CardState.WARNING) {
      // Optional parameter alwaysShowDialog is set to true to always show the
      // confirmation dialog regardless of the incognito windows open.
      this.performRestart(RestartType.RELAUNCH, true);
    } else {
      Router.getInstance().navigateTo(
          routes.ABOUT, /* dynamicParams= */ undefined,
          /* removeSearch= */ true);
    }
  }

  protected onShModuleItemLinkClick_(event: CustomEvent<HTMLAnchorElement>) {
    this.browserProxy_.recordSafetyHubInteraction();
    const headerString =
        event.detail.querySelector('.site-representation')!.textContent;

    switch (headerString) {
      case this.i18n('safetyHubUserEduDataHeader'):
        this.metricsBrowserProxy_.recordAction(
            'Settings.SafetyHub.SafetyToolsLinkClicked');
        break;
      case this.i18n('safetyHubUserEduIncognitoHeader'):
        this.metricsBrowserProxy_.recordAction(
            'Settings.SafetyHub.IncognitoLinkClicked');
        break;
      case this.i18n('safetyHubUserEduSafeBrowsingHeader'):
        this.metricsBrowserProxy_.recordAction(
            'Settings.SafetyHub.SafeBrowsingLinkClicked');
        break;
      default:
        assertNotReached();
    }
  }

  protected onVersionKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onVersionClick_();
    }
  }

  private onSafeBrowsingPrefChanged_() {
    if (this.safeBrowsingPref_ === undefined) {
      return;
    }
    this.browserProxy_.getSafeBrowsingCardData().then((data: CardInfo) => {
      this.safeBrowsingCardData_ = data;
    });
  }

  protected onSafeBrowsingClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.SafeBrowsingCard.StatusOnClick',
        this.safeBrowsingCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  protected onSafeBrowsingKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onSafeBrowsingClick_();
    }
  }

  private onNotificationPermissionListChanged_(permissions:
                                                   NotificationPermission[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showNotificationPermissions_ =
        permissions.length > 0 || this.showNotificationPermissions_;
    this.hasDataForNotificationPermissions_ = true;
  }

  private onUnusedSitePermissionListChanged_(permissions:
                                                 UnusedSitePermissions[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showUnusedSitePermissions_ =
        permissions.length > 0 || this.showUnusedSitePermissions_;
    this.hasDataForUnusedPermissions_ = true;
  }

  protected shouldShowNoRecommendationsState_(): boolean {
    return !(
        this.showUnusedSitePermissions_ || this.showNotificationPermissions_ ||
        this.showExtensions_);
  }

  private onExtensionsChanged_(numberOfExtensions: number) {
    this.showExtensions_ = !!numberOfExtensions;
    this.hasDataForExtensions_ = true;
  }

  protected computeVersionCardRole_(): string {
    return this.versionCardData_.state === CardState.WARNING ? 'button' :
                                                               'link';
  }

  protected computeVersionCardAriaDescription_(): string {
    return this.versionCardData_.state === CardState.WARNING ?
        this.i18n('safetyHubVersionRelaunchAriaLabel') :
        this.i18n('safetyHubVersionNavigationAriaLabel');
  }

  private isEnterOrSpaceClicked_(e: KeyboardEvent): boolean {
    return e.key === 'Enter' || e.key === ' ';
  }

  private onAllModulesLoaded_() {
    // If the metrics are recorded already, don't record again.
    if (!this.shouldRecordMetric_) {
      return;
    }

    // Wait till the data of the cards be ready.
    if (!this.passwordCardData_ || !this.safeBrowsingCardData_ ||
        !this.versionCardData_) {
      return;
    }

    // Wait till the data of the modules be ready.
    if (!this.hasDataForUnusedPermissions_ ||
        !this.hasDataForNotificationPermissions_ ||
        !this.hasDataForExtensions_) {
      return;
    }

    this.shouldRecordMetric_ = false;
    let hasAnyWarning: boolean = false;
    // TODO(crbug.com/40267370): Iterate over the cards/modules with for loop.
    if (this.passwordCardData_.state !== CardState.SAFE) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.PASSWORDS);
      hasAnyWarning = true;
    }

    if (this.safeBrowsingCardData_.state !== CardState.SAFE) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.SAFE_BROWSING);
      hasAnyWarning = true;
    }

    if (this.versionCardData_.state !== CardState.SAFE) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.VERSION);
      hasAnyWarning = true;
    }

    if (this.showNotificationPermissions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.NOTIFICATIONS);
      hasAnyWarning = true;
    }

    if (this.showUnusedSitePermissions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.PERMISSIONS);
      hasAnyWarning = true;
    }

    if (this.showExtensions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.EXTENSIONS);
      hasAnyWarning = true;
    }

    this.metricsBrowserProxy_.recordSafetyHubDashboardAnyWarning(hasAnyWarning);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

export type SafetyHubPageElement = SettingsSafetyHubPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);
