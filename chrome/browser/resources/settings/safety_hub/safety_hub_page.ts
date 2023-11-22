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

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyHubCardState, SafetyHubSurfaces} from '../metrics_browser_proxy.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';

import {CardInfo, CardState, NotificationPermission, SafetyHubBrowserProxy, SafetyHubBrowserProxyImpl, SafetyHubEvent, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {SiteInfo} from './safety_hub_module.js';
import {getTemplate} from './safety_hub_page.html.js';

export interface SettingsSafetyHubPageElement {
  $: {
    passwords: HTMLElement,
    safeBrowsing: HTMLElement,
    version: HTMLElement,
  };
}

const SettingsSafetyHubPageElementBase = RouteObserverMixin(
    RelaunchMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

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
    };
  }

  private passwordCardData_: CardInfo;
  private versionCardData_: CardInfo;
  private safeBrowsingCardData_: CardInfo;
  private showNotificationPermissions_: boolean;
  private showUnusedSitePermissions_: boolean;
  private showNoRecommendationsState_: boolean;
  private showExtensions_: boolean;
  private userEducationItemList_: SiteInfo[];
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.initializeCards_();
    this.initializeModules_();
    this.initializeUserEducation_();
  }

  override currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() !== routes.SAFETY_HUB) {
      return;
    }
    // When the user navigates to the Safety Hub page, any active menu
    // notification is dismissed.
    this.browserProxy_.dismissActiveMenuNotification();

    this.metricsBrowserProxy_.recordSafetyHubImpression(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);
    this.metricsBrowserProxy_.recordSafetyHubInteraction(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);
  }

  private initializeCards_() {
    // TODO(1443466): Add listeners for cards.
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

    if (this.versionCardData_.state === CardState.WARNING) {
      this.performRestart(RestartType.RELAUNCH);
    } else {
      Router.getInstance().navigateTo(
          routes.ABOUT, /* dynamicParams= */ undefined,
          /* removeSearch= */ true);
    }
  }

  private onVersionKeyPress_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onVersionClick_();
    }
  }

  private onSafeBrowsingClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.SafeBrowsingCard.StatusOnClick',
        this.safeBrowsingCardData_.state as unknown as SafetyHubCardState);

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
  }

  private onUnusedSitePermissionListChanged_(permissions:
                                                 UnusedSitePermissions[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showUnusedSitePermissions_ =
        permissions.length > 0 || this.showUnusedSitePermissions_;
  }

  private computeShowNoRecommendationsState_(): boolean {
    return !(
        this.showUnusedSitePermissions_ || this.showNotificationPermissions_ ||
        this.showExtensions_);
  }

  private onExtensionsChanged_(numberOfExtensions: number) {
    this.showExtensions_ = !!numberOfExtensions;
  }

  private isEnterOrSpaceClicked_(e: KeyboardEvent): boolean {
    return e.key === 'Enter' || e.key === ' ';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);
