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
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
import '../safety_hub/safety_hub_module.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/geolocation_page.js';
import '../site_settings/notifications_page.js';
import '../site_settings/settings_category_default_radio_group.js';
import '../site_settings/smart_card_readers_page.js';
import './privacy_guide/privacy_guide_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {PrivacyPageBrowserProxy} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
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

interface BlockAutoplayStatus {
  enabled: boolean;
  pref: chrome.settingsPrivate.PrefObject<boolean>;
}

export interface SettingsPrivacyPageElement {
  $: {
    clearBrowsingData: CrLinkRowElement,
    permissionsLinkRow: CrLinkRowElement,
    securityLinkRow: CrLinkRowElement,
  };
}

const SettingsPrivacyPageElementBase =
    PrivacyGuideAvailabilityMixin(RouteObserverMixin(
        WebUiListenerMixin(I18nMixin(PrefsMixin(BaseMixin(PolymerElement))))));

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

      enableSafeBrowsingSubresourceFilter_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
        },
      },

      enableBlockAutoplayContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
        },
      },

      enableManagePhones_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysManagePhones');
        },
      },

      blockAutoplayStatus_: {
        type: Object,
        value() {
          return {};
        },
      },

      enableDeleteBrowsingDataRevamp_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableDeleteBrowsingDataRevamp'),
      },

      enablePaymentHandlerContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
        },
      },

      enableHandTrackingContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableHandTrackingContentSetting');
        },
      },

      enableFederatedIdentityApiContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableFederatedIdentityApiContentSetting');
        },
      },

      enableExperimentalWebPlatformFeatures_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableExperimentalWebPlatformFeatures');
        },
      },

      enableSecurityKeysSubpage_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysSubpage');
        },
      },

      // <if expr="is_chromeos">
      enableSmartCardReadersContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableSmartCardReadersContentSetting');
        },
      },
      // </if>

      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },

      enableWebPrintingContentSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebPrintingContentSetting'),
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

      autoPictureInPictureEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('autoPictureInPictureEnabled'),
      },

      capturedSurfaceControlEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('capturedSurfaceControlEnabled'),
      },

      /**
       * Whether the File System Access Persistent Permissions UI should be
       * displayed.
       */
      showPersistentPermissions_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPersistentPermissions');
        },
      },

      enableAutomaticFullscreenContentSetting_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableAutomaticFullscreenContentSetting'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (routes.SECURITY) {
            map.set(routes.SECURITY.path, '#securityLinkRow');
          }

          if (routes.COOKIES) {
            map.set(
                `${routes.COOKIES.path}_${routes.PRIVACY.path}`,
                '#thirdPartyCookiesLinkRow');
            map.set(
                `${routes.COOKIES.path}_${routes.BASIC.path}`,
                '#thirdPartyCookiesLinkRow');
          }

          if (routes.SITE_SETTINGS) {
            map.set(routes.SITE_SETTINGS.path, '#permissionsLinkRow');
          }

          if (routes.PRIVACY_GUIDE) {
            map.set(routes.PRIVACY_GUIDE.path, '#privacyGuideLinkRow');
          }

          if (routes.PRIVACY_SANDBOX) {
            map.set(routes.PRIVACY_SANDBOX.path, '#privacySandboxLinkRow');
          }

          if (routes.INCOGNITO_TRACKING_PROTECTIONS) {
            map.set(routes.INCOGNITO_TRACKING_PROTECTIONS.path,
              '#incognitoTrackingProtectionsLinkRow');
          }

          return map;
        },
      },

      searchFilter_: {
        type: String,
        value: '',
        observer: 'updateAllSitesPageTitle_',
      },

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

      shouldShowSafetyHub_: {
        type: Boolean,
        value() {
          return !loadTimeData.getBoolean('isGuest');
        },
      },

      enableKeyboardLockPrompt_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableKeyboardLockPrompt'),
      },

      enableWebAppInstallation_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebAppInstallation'),
      },

      enableRelatedWebsiteSetsV2Ui_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isRelatedWebsiteSetsV2UiEnabled'),
      },

      enableLocalNetworkAccessSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableLocalNetworkAccessSetting'),
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

      allSitesPageTitle_: String,
    };
  }

  declare private isGuest_: boolean;
  declare private showPersistentPermissions_: boolean;
  declare private showClearBrowsingDataDialog_: boolean;
  declare private showPrivacyGuideDialog_: boolean;
  declare private enableSafeBrowsingSubresourceFilter_: boolean;
  declare private enableBlockAutoplayContentSetting_: boolean;
  declare private enableManagePhones_: boolean;
  declare private blockAutoplayStatus_: BlockAutoplayStatus;
  declare private enableDeleteBrowsingDataRevamp_: boolean;
  declare private enableFederatedIdentityApiContentSetting_: boolean;
  declare private enablePaymentHandlerContentSetting_: boolean;
  declare private enableHandTrackingContentSetting_: boolean;
  declare private enableExperimentalWebPlatformFeatures_: boolean;
  declare private enableSecurityKeysSubpage_: boolean;
  // <if expr="is_chromeos">
  declare private enableSmartCardReadersContentSetting_: boolean;
  // </if>
  declare private enableWebBluetoothNewPermissionsBackend_: boolean;
  declare private enableWebPrintingContentSetting_: boolean;
  declare private isPrivacySandboxRestricted_: boolean;
  declare private isPrivacySandboxRestrictedNoticeEnabled_: boolean;
  declare private enableAutomaticFullscreenContentSetting_: boolean;
  private privateStateTokensEnabled_: boolean;
  declare private autoPictureInPictureEnabled_: boolean;
  declare private capturedSurfaceControlEnabled_: boolean;
  declare private shouldShowSafetyHub_: boolean;
  declare private enableWebAppInstallation_: boolean;
  declare private enableLocalNetworkAccessSetting_: boolean;
  declare private focusConfig_: FocusConfig;
  declare private searchFilter_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteSettingsPrefsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  declare private enableKeyboardLockPrompt_: boolean;
  declare private enableRelatedWebsiteSetsV2Ui_: boolean;
  declare private allSitesPageTitle_: string;
  declare private enableIncognitoTrackingProtections_: boolean;
  declare private enableBundledSecuritySettings_: boolean;

  override ready() {
    super.ready();

    this.onBlockAutoplayStatusChanged_({
      pref: {
        key: '',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      enabled: false,
    });

    this.addWebUiListener(
        'onBlockAutoplayStatusChanged',
        (status: BlockAutoplayStatus) =>
            this.onBlockAutoplayStatusChanged_(status));

    this.updateAllSitesPageTitle_();
  }

  override currentRouteChanged() {
    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
    this.showPrivacyGuideDialog_ =
        Router.getInstance().getCurrentRoute() === routes.PRIVACY_GUIDE &&
        this.isPrivacyGuideAvailable;
  }

  /**
   * Called when the block autoplay status changes.
   */
  private onBlockAutoplayStatusChanged_(autoplayStatus: BlockAutoplayStatus) {
    this.blockAutoplayStatus_ = autoplayStatus;
  }

  /**
   * Updates the block autoplay pref when the toggle is changed.
   */
  private onBlockAutoplayToggleChange_(event: Event) {
    const target = event.target as SettingsToggleButtonElement;
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
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
    // TODO(crbug.com/408036586): Add user action for Incognito tracking protections row click.
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
        return this.i18n('thirdPartyCookiesLinkRowSublabelEnabled');
      case CookieControlsMode.INCOGNITO_ONLY:
        return loadTimeData.getBoolean('isAlwaysBlock3pcsIncognitoEnabled') ?
            this.i18n('thirdPartyCookiesLinkRowSublabelEnabled') :
            this.i18n('thirdPartyCookiesLinkRowSublabelDisabledIncognito');
      case CookieControlsMode.BLOCK_THIRD_PARTY:
        return this.i18n('thirdPartyCookiesLinkRowSublabelDisabled');
      default:
        assertNotReached();
    }
  }

  private updateAllSitesPageTitle_(): void {
    const rwsPrefix = 'related:';
    if (this.enableRelatedWebsiteSetsV2Ui_ &&
        this.searchFilter_.length > rwsPrefix.length &&
        this.searchFilter_.startsWith(rwsPrefix)) {
      this.allSitesPageTitle_ = loadTimeData.getStringF(
          'allSitesRwsFilterViewTitle',
          this.searchFilter_.substring(rwsPrefix.length));
    } else {
      this.allSitesPageTitle_ = this.i18n('siteSettingsAllSites');
    }
  }

  private shouldShowAdPrivacy_(): boolean {
    return !this.isPrivacySandboxRestricted_ ||
        this.isPrivacySandboxRestrictedNoticeEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page': SettingsPrivacyPageElement;
  }
}

customElements.define(
    SettingsPrivacyPageElement.is, SettingsPrivacyPageElement);
