// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../safety_hub/safety_hub_module.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/settings_category_default_radio_group.js';
import './privacy_guide/privacy_guide_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {FocusConfig} from '../focus_config.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl} from '../safety_hub/safety_hub_browser_proxy.js';
import {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode} from '../site_settings/constants.js';
import type {SiteSettingsPrefsBrowserProxy} from '../site_settings/site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyGuideAvailabilityMixin} from './privacy_guide/privacy_guide_availability_mixin.js';
import {getTemplate} from './privacy_page.html.js';

export interface SettingsPrivacyPageElement {
  $: {
    clearBrowsingData: CrLinkRowElement,
    permissionsLinkRow: CrLinkRowElement,
    securityLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacyPageElementBase = PrivacyGuideAvailabilityMixin(
    RouteObserverMixin(I18nMixin(PrefsMixin(BaseMixin(PolymerElement)))));

export class SettingsPrivacyPageElement extends SettingsPrivacyPageElementBase {
  static get is() {
    return 'settings-privacy-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      showClearBrowsingDataDialog_: Boolean,
      showPrivacyGuideDialog_: Boolean,

      enableDeleteBrowsingDataRevamp_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableDeleteBrowsingDataRevamp'),
      },

      enableExperimentalWebPlatformFeatures_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableExperimentalWebPlatformFeatures');
        },
      },

      isPrivacySandboxRestricted_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxRestricted'),
      },

      isPrivacySandboxRestrictedNoticeEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isPrivacySandboxRestrictedNoticeEnabled'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (routes.SECURITY) {
            map.set(routes.SECURITY.path, '#securityLinkRow');
          }

          if (routes.PRIVACY_GUIDE) {
            map.set(routes.PRIVACY_GUIDE.path, '#privacyGuideLinkRow');
          }

          return map;
        },
      },

      searchFilter_: String,

      /**
       * Expose ContentSettingsTypes enum to HTML bindings.
       */
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      /**
       * Expose ContentSetting enum to HTML bindings.
       */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      /**
       * Expose ChooserType enum to HTML bindings.
       */
      chooserTypeEnum_: {
        type: Object,
        value: ChooserType,
      },

      enableIncognitoTrackingProtections_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableIncognitoTrackingProtections'),
      },

      enableBundledSecuritySettings_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableBundledSecuritySettings'),
      },

      // The label of the confirmation toast that is displayed after deletion
      // from 'Delete Browsing data' is completed.
      dbdDeletionConfirmationToastLabel_: {
        type: String,
        value: '',
      },

      shouldShowDbdDeletionConfirmationToast_: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare private isGuest_: boolean;
  declare private showClearBrowsingDataDialog_: boolean;
  declare private showPrivacyGuideDialog_: boolean;
  declare private enableDeleteBrowsingDataRevamp_: boolean;
  declare private enableExperimentalWebPlatformFeatures_: boolean;
  declare private isPrivacySandboxRestricted_: boolean;
  declare private isPrivacySandboxRestrictedNoticeEnabled_: boolean;
  private privateStateTokensEnabled_: boolean;
  declare private focusConfig_: FocusConfig;
  declare private searchFilter_: string;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteSettingsPrefsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  declare private enableIncognitoTrackingProtections_: boolean;
  declare private enableBundledSecuritySettings_: boolean;
  declare private dbdDeletionConfirmationToastLabel_: string;
  declare private shouldShowDbdDeletionConfirmationToast_: boolean;

  override currentRouteChanged() {
    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
    this.showPrivacyGuideDialog_ =
        Router.getInstance().getCurrentRoute() === routes.PRIVACY_GUIDE &&
        this.isPrivacyGuideAvailable;
  }

  private onClearBrowsingDataClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
  }

  private onCookiesClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.COOKIES);
  }

  private onCbdDialogClosed_() {
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA.parent!);

    if (this.shouldShowDbdDeletionConfirmationToast_) {
      assert(this.dbdDeletionConfirmationToastLabel_);
      const toast = this.shadowRoot!.querySelector<CrToastElement>(
          '#deleteBrowsingDataToast');
      assert(toast);
      toast.show();
      this.shouldShowDbdDeletionConfirmationToast_ = false;
    }

    setTimeout(() => {
      // Focus after a timeout to ensure any a11y messages get read before
      // screen readers read out the newly focused element.
      const toFocus =
          this.shadowRoot!.querySelector<HTMLElement>('#clearBrowsingData');
      assert(toFocus);
      focusWithoutInk(toFocus);
    });
  }

  private onPrivacyGuideDialogClosed_() {
    Router.getInstance().navigateToPreviousRoute();
    const toFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#privacyGuideLinkRow');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private onPermissionsPageClick_() {
    this.interactedWithPage_();

    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
  }

  private onSecurityPageClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromParentSettings');
    Router.getInstance().navigateTo(routes.SECURITY);
  }

  private onPrivacySandboxClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacySandbox.OpenedFromSettingsParent');
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX);
  }

  private onIncognitoTrackingProtectionsClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'Settings.TrackingProtections.OpenedFromPrivacyPage');
    Router.getInstance().navigateTo(routes.INCOGNITO_TRACKING_PROTECTIONS);
  }

  private onPrivacyGuideClick_() {
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

  private computeAdPrivacySublabel_(): string {
    // When the privacy sandbox is restricted with a notice, the sublabel
    // wording indicates measurement only, rather than general ad privacy.
    const restricted = this.isPrivacySandboxRestricted_ &&
        this.isPrivacySandboxRestrictedNoticeEnabled_;
    return restricted ? this.i18n('adPrivacyRestrictedLinkRowSubLabel') :
                        this.i18n('adPrivacyLinkRowSubLabel');
  }

  private computeThirdPartyCookiesSublabel_(): string {
    // Handle the correct pref in Mode B.
    if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled')) {
      if (this.getPref('tracking_protection.block_all_3pc_toggle_enabled')
              .value) {
        return this.i18n('thirdPartyCookiesLinkRowSublabelDisabled');
      }
      return this.i18n('thirdPartyCookiesLinkRowSublabelLimited');
    }
    const currentCookieSetting =
        this.getPref('profile.cookie_controls_mode').value;
    switch (currentCookieSetting) {
      case CookieControlsMode.OFF:
      case CookieControlsMode.INCOGNITO_ONLY:
        return this.i18n('thirdPartyCookiesLinkRowSublabelEnabled');
      case CookieControlsMode.BLOCK_THIRD_PARTY:
        return this.i18n('thirdPartyCookiesLinkRowSublabelDisabled');
      default:
        assertNotReached();
    }
  }

  private shouldShowAdPrivacy_(): boolean {
    return !this.isPrivacySandboxRestricted_ ||
        this.isPrivacySandboxRestrictedNoticeEnabled_;
  }

  private onBrowsingDataDeleted_(
      e: CustomEvent<{deletionConfirmationText: string}>) {
    this.dbdDeletionConfirmationToastLabel_ = e.detail.deletionConfirmationText;
    this.shouldShowDbdDeletionConfirmationToast_ = true;
  }

  getAssociatedControlFor(childViewId: string): HTMLElement {
    let triggerId: string|null = null;
    switch (childViewId) {
      case 'cookies':
        triggerId = 'thirdPartyCookiesLinkRow';
        break;
      case 'incognitoTrackingProtections':
        triggerId = 'incognitoTrackingProtectionsLinkRow';
        break;
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
      case 'siteSettingsHandlers':
      case 'siteSettingsHandTracking':
      case 'siteSettingsHidDevices':
      case 'siteSettingsIdleDetection':
      case 'siteSettingsImages':
      case 'siteSettingsJavascript':
      case 'siteSettingsJavascriptOptimizer':
      case 'siteSettingsKeyboardLock':
      case 'siteSettingsLocalFonts':
      case 'siteSettingsLocalNetworkAccess':
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
      // </if>
      case 'siteSettingsSound':
      case 'siteSettingsStorageAccess':
      case 'siteSettingsUsbDevices':
      case 'siteSettingsVr':
      case 'siteSettingsWebAppInstallation':
      case 'siteSettingsWebPrinting':
      case 'siteSettingsWindowManagement':
      case 'siteSettingsZoomLevels':
        triggerId = 'permissionsLinkRow';
        break;
      case 'privacySandbox':
      case 'privacySandboxAdMeasurement':
      case 'privacySandboxFledge':
      case 'privacySandboxManageTopics':
      case 'privacySandboxTopics':
        triggerId = 'privacySandboxLinkRow';
        break;
      // TODO(crbug.com/424223101): Add more child view IDs as they
      // are migrated to the new architecture.
      default:
        assertNotReached();
    }

    assert(triggerId);

    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
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
