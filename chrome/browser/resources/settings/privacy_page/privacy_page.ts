// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../settings_page/settings_section.js';
import './privacy_guide/privacy_guide_dialog.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {getCss as getCrHiddenStyleCss} from 'chrome://resources/cr_elements/cr_hidden_style_lit.css.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import type {Route} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';
import {CookieControlsMode} from '../site_settings/constants.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from './hats_browser_proxy.js';
import {PrivacyGuideAvailabilityMixinLit} from './privacy_guide/privacy_guide_availability_mixin_lit.js';
import {getHtml} from './privacy_page.html.js';

export interface SettingsPrivacyPageElement {
  $: {
    clearBrowsingData: CrLinkRowElement,
    deleteBrowsingDataToast: CrToastElement,
    siteSettingsLinkRow: CrLinkRowElement,
    securityLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacyPageElementBase =
    PrivacyGuideAvailabilityMixinLit(SettingsViewMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement))));

export type PrivacyPageElement = SettingsPrivacyPageElement;

export class SettingsPrivacyPageElement extends SettingsPrivacyPageElementBase {
  static get is() {
    return 'settings-privacy-page';
  }

  static override get styles() {
    return [
      getCrSharedStyleCss(),
      getCrHiddenStyleCss(),
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showClearBrowsingDataDialog_: {type: Boolean},
      showPrivacyGuideDialog_: {type: Boolean},

      // The label of the confirmation toast that is displayed after deletion
      // from 'Delete Browsing data' is completed.
      dbdDeletionConfirmationToastLabel_: {type: String},

      showUniversalOptOutSettings_: {type: Boolean},

      cookieControlsModePref_: {type: Object},
    };
  }

  protected accessor showClearBrowsingDataDialog_: boolean = false;
  protected accessor showPrivacyGuideDialog_: boolean = false;
  protected accessor dbdDeletionConfirmationToastLabel_: string = '';
  private accessor showUniversalOptOutSettings_: boolean =
      loadTimeData.getBoolean('showUniversalOptOutSettings');
  private accessor cookieControlsModePref_:
      chrome.settingsPrivate.PrefObject<CookieControlsMode>|undefined;

  private shouldShowDbdDeletionConfirmationToast_: boolean = false;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('profile.cookie_controls_mode', 'cookieControlsModePref_');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
    this.showPrivacyGuideDialog_ =
        Router.getInstance().getCurrentRoute() === routes.PRIVACY_GUIDE &&
        this.isPrivacyGuideAvailable;
  }

  protected onClearBrowsingDataClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
  }

  protected onCookiesClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.COOKIES);
  }

  protected onCbdDialogClose_() {
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA.parent!);

    if (this.shouldShowDbdDeletionConfirmationToast_) {
      assert(this.dbdDeletionConfirmationToastLabel_);
      this.$.deleteBrowsingDataToast.show();
      this.shouldShowDbdDeletionConfirmationToast_ = false;
    }

    this.updateComplete.then(() => {
      // Focus after next render has completed to ensure any a11y messages get
      // read and the UI has updated before screen readers read out the newly
      // focused element.
      focusWithoutInk(this.$.clearBrowsingData);
    });
  }

  protected onPrivacyGuideDialogClose_() {
    Router.getInstance().navigateToPreviousRoute();
    const toFocus =
        this.shadowRoot.querySelector<HTMLElement>('#privacyGuideLinkRow');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  protected onSiteSettingsLinkRowClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
  }

  protected onSecurityPageClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromParentSettings');
    Router.getInstance().navigateTo(routes.SECURITY);
  }

  protected onPrivacyGuideClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.StartPrivacySettings');
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE, /* dynamicParams */ undefined,
        /* removeSearch */ true);
  }

  private interactedWithPage_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.USED_PRIVACY_CARD);
  }

  protected getThirdPartyCookiesLabel_(): string {
    return this.i18n(
        this.showUniversalOptOutSettings_ ?
            'thirdPartyCookiesAndSiteDataLinkRowLabel' :
            'thirdPartyCookiesLinkRowLabel');
  }

  protected getThirdPartyCookiesSublabel_(): string {
    if (this.showUniversalOptOutSettings_) {
      return this.i18n('thirdPartyCookiesAndSiteDataLinkRowSublabel');
    }

    if (!this.cookieControlsModePref_) {
      return '';
    }

    switch (this.cookieControlsModePref_.value) {
      case CookieControlsMode.OFF:
      case CookieControlsMode.INCOGNITO_ONLY:
        return this.i18n('thirdPartyCookiesLinkRowSublabelEnabled');
      case CookieControlsMode.BLOCK_THIRD_PARTY:
        return this.i18n('thirdPartyCookiesLinkRowSublabelDisabled');
      default:
        assertNotReached();
    }
  }

  protected onBrowsingDataDeleted_(
      e: CustomEvent<{deletionConfirmationText: string}>) {
    this.dbdDeletionConfirmationToastLabel_ = e.detail.deletionConfirmationText;
    this.shouldShowDbdDeletionConfirmationToast_ = true;
  }

  // SettingsViewMixinLit implementation.
  override getFocusConfig() {
    const map = new Map();

    if (routes.COOKIES) {
      map.set(routes.COOKIES.path, '#thirdPartyCookiesLinkRow');
    }

    if (routes.PRIVACY_GUIDE) {
      map.set(routes.PRIVACY_GUIDE.path, '#privacyGuideLinkRow');
    }

    if (routes.SECURITY) {
      map.set(routes.SECURITY.path, '#securityLinkRow');
    }

    if (routes.SITE_SETTINGS) {
      map.set(routes.SITE_SETTINGS.path, '#siteSettingsLinkRow');
    }

    return map;
  }

  // SettingsViewMixinLit implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    let triggerId: string|null = null;
    switch (childViewId) {
      case 'cookies':
        triggerId = 'thirdPartyCookiesLinkRow';
        break;
      case 'security':
      case 'securityKeys':
        triggerId = 'securityLinkRow';
        break;
      case 'siteSettings':
      case 'siteSettingsAds':
      case 'siteSettingsAll':
      case 'siteSettingsAr':
      case 'siteSettingsAutomaticDownloads':
      case 'siteSettingsAutomaticFullscreen':
      case 'siteSettingsAutoPictureInPicture':
      case 'siteSettingsAutoVerify':
      case 'siteSettingsBackgroundSync':
      case 'siteSettingsBluetoothDevices':
      case 'siteSettingsBluetoothScanning':
      case 'siteSettingsCamera':
      case 'siteSettingsCapturedSurfaceControl':
      case 'siteSettingsClipboard':
      case 'siteSettingsFederatedIdentityApi':
      case 'siteSettingsFilesystemWrite':
      case 'siteSettingsFilesystemWriteDetails':
      case 'siteSettingsHandlers':
      case 'siteSettingsHandTracking':
      case 'siteSettingsHidDevices':
      case 'siteSettingsIdleDetection':
      case 'siteSettingsImages':
      case 'siteSettingsInlineCueMenu':
      case 'siteSettingsJavascript':
      case 'siteSettingsJavascriptOptimizer':
      case 'siteSettingsKeyboardLock':
      case 'siteSettingsLocalFonts':
      case 'siteSettingsLocalNetwork':
      case 'siteSettingsLocalNetworkAccess':
      case 'siteSettingsLoopbackNetwork':
      case 'siteSettingsLocation':
      case 'siteSettingsMicrophone':
      case 'siteSettingsMidiDevices':
      case 'siteSettingsMixedscript':
      case 'siteSettingsNotifications':
      case 'siteSettingsPaymentHandler':
      case 'siteSettingsPdfDocuments':
      case 'siteSettingsPopups':
      case 'siteSettingsProtectedContent':
      case 'siteSettingsSensors':
      case 'siteSettingsSerialPorts':
      case 'siteSettingsSiteData':
      case 'siteSettingsSiteDetails':
      // <if expr="is_chromeos">
      case 'siteSettingsSmartCardReaders':
      case 'siteSettingsWebPrinting':
      // </if>
      case 'siteSettingsSound':
      case 'siteSettingsStorageAccess':
      case 'siteSettingsUsbDevices':
      case 'siteSettingsVr':
      case 'siteSettingsWebAppInstallation':
      case 'siteSettingsWindowManagement':
      case 'siteSettingsZoomLevels':
        triggerId = 'siteSettingsLinkRow';
        break;
      default:
        assertNotReached();
    }

    assert(triggerId);

    const control = this.shadowRoot.querySelector<HTMLElement>(`#${triggerId}`);
    assert(
        control,
        `Failed to find associated control for child '${childViewId}'`);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page': SettingsPrivacyPageElement;
  }
}

customElements.define(
    SettingsPrivacyPageElement.is, SettingsPrivacyPageElement);
