// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './safety_hub_card.js';
import './safety_hub_module.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import type {MetricsBrowserProxy, SafetyHubCardState} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubModuleType, SafetyHubSurfaces} from '../metrics_browser_proxy.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';

import type {CardInfo, NotificationPermission, SafetyHubBrowserProxy, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {CardState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from './safety_hub_browser_proxy.js';
import type {SiteInfo} from './safety_hub_module.js';
import {getTemplate} from './safety_hub_page.html.js';

export interface SettingsSafetyHubPageElement {
  $: {
    passwords: HTMLElement,
    safeBrowsing: HTMLElement,
    version: HTMLElement,
  };
}

const SettingsSafetyHubPageElementBase = RouteObserverMixin(
    RelaunchMixin(PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsSafetyHubPageElement extends
    SettingsSafetyHubPageElementBase {
  static get is() {
    return 'settings-safety-hub-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The object that holds data of Password Check card.
      passwordCardData_: Object,

      // The object that holds data of Version Check card.
      versionCardData_: Object,

      // The object that holds data of Safe Browsing card.
      safeBrowsingCardData_: Object,

      // Whether Notification Permissions module should be visible.
      showNotificationPermissions_: {
        type: Boolean,
        value: false,
      },

      // Whether Unused Site Permissions module should be visible.
      showUnusedSitePermissions_: {
        type: Boolean,
        value: false,
      },

      // Whether Extensions module should be visible.
      showExtensions_: {
        type: Boolean,
        value: false,
      },

      showNoRecommendationsState_: {
        type: Boolean,
        computed:
            'computeShowNoRecommendationsState_(showUnusedSitePermissions_.*, showExtensions_.*, showNotificationPermissions_.*)',
      },

      userEducationItemList_: Array,

      // Whether the data for notification permissions is ready.
      hasDataForNotificationPermissions_: Boolean,

      // Whether the data for unused site permissions is ready.
      hasDataForUnusedPermissions_: Boolean,

      // Whether the data for extensions is ready.
      hasDataForExtensions_: Boolean,

      // String that identifies version card's role announced by accessibility
      // voiceover.
      versionCardRole_: {
        type: String,
        computed: 'computeVersionCardRole_(versionCardData_)',
      },

      // String that identifies version card's description announced by
      // accessibility voiceover.
      versionCardAriaDescription_: {
        type: String,
        computed: 'computeVersionCardAriaDescription_(versionCardData_)',
      },

    };
  }

  static get observers() {
    return [
      'onAllModulesLoaded_(passwordCardData_, versionCardData_, safeBrowsingCardData_, hasDataForUnusedPermissions_, hasDataForNotificationPermissions_, hasDataForExtensions_)',
      'onSafeBrowsingPrefChanged_(prefs.generated.safe_browsing)',
    ];
  }

  private passwordCardData_: CardInfo;
  private versionCardData_: CardInfo;
  private safeBrowsingCardData_: CardInfo;
  private showNotificationPermissions_: boolean;
  private hasDataForNotificationPermissions_: boolean;
  private showUnusedSitePermissions_: boolean;
  private hasDataForUnusedPermissions_: boolean;
  private showNoRecommendationsState_: boolean;
  private showExtensions_: boolean;
  private hasDataForExtensions_: boolean;
  private shouldRecordMetric_: boolean = false;
  private userEducationItemList_: SiteInfo[];
  private versionCardRole_: string;
  private versionCardAriaDescription_: string;
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    this.initializeCards_();
    this.initializeModules_();
    this.initializeUserEducation_();

    super.connectedCallback();
  }

  override currentRouteChanged() {
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
        icon: 'settings20:chrome-filled',
      },
      {
        origin: this.i18n('safetyHubUserEduIncognitoHeader'),
        detail: this.i18nAdvanced('safetyHubUserEduIncognitoSubheader'),
        icon: 'settings20:incognito-unfilled',
      },
      {
        origin: this.i18n('safetyHubUserEduSafeBrowsingHeader'),
        detail: this.i18nAdvanced('safetyHubUserEduSafeBrowsingSubheader'),
        icon: 'cr:security',
      },
    ];
  }

  private onPasswordsClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.PasswordsCard.StatusOnClick',
        this.passwordCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.CHECKUP);
  }

  private onPasswordsKeyPress_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onPasswordsClick_();
    }
  }

  private onVersionClick_() {
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

  private onEducationLinkClick_(event: CustomEvent<HTMLAnchorElement>) {
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

  private onVersionKeyPress_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onVersionClick_();
    }
  }

  private onSafeBrowsingPrefChanged_() {
    this.browserProxy_.getSafeBrowsingCardData().then((data: CardInfo) => {
      this.safeBrowsingCardData_ = data;
    });
  }

  private onSafeBrowsingClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.SafeBrowsingCard.StatusOnClick',
        this.safeBrowsingCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  private onSafeBrowsingKeyPress_(e: KeyboardEvent) {
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

  private computeShowNoRecommendationsState_(): boolean {
    return !(
        this.showUnusedSitePermissions_ || this.showNotificationPermissions_ ||
        this.showExtensions_);
  }

  private onExtensionsChanged_(numberOfExtensions: number) {
    this.showExtensions_ = !!numberOfExtensions;
    this.hasDataForExtensions_ = true;
  }

  private computeVersionCardRole_(): string {
    return this.versionCardData_.state === CardState.WARNING ? 'button' : 'link';
  }

  private computeVersionCardAriaDescription_(): string {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);
