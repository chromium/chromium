// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../privacy_icons.html.js';
import '../safety_hub/safety_hub_module.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
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
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import type {FocusConfig} from '../focus_config.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions, SafetyHubEntryPoint} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {NotificationPermission, SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode, SettingsState} from '../site_settings/constants.js';
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
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

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

      blockAutoplayStatus_: {
        type: Object,
        value() {
          return {};
        },
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

      enableSmartCardReadersContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableSmartCardReadersContentSetting');
        },
      },

      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },

      enableWebPrintingContentSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebPrintingContentSetting'),
      },

      showNotificationPermissionsReview_: {
        type: Boolean,
        value: false,
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

      is3pcdRedesignEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
                     'is3pcdCookieSettingsRedesignEnabled') &&
              loadTimeData.getBoolean('isTrackingProtectionUxEnabled');
        },
      },

      privateStateTokensEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privateStateTokensEnabled'),
      },

      autoPictureInPictureEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('autoPictureInPictureEnabled'),
      },

      capturedSurfaceControlEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('capturedSurfaceControlEnabled'),
      },

      enableAiSettingsPageRefresh_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableAiSettingsPageRefresh'),
      },

      enableComposeProactiveNudge_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableComposeProactiveNudge'),
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

      // TODO(b/371166959): Cleanup privacy page by removing
      // isProactiveTopicsBlockingEnabled_.
      isProactiveTopicsBlockingEnabled_: {
        type: Boolean,
        value: true,
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

          if (routes.TRACKING_PROTECTION) {
            map.set(
                routes.TRACKING_PROTECTION.path, '#trackingProtectionLinkRow');
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

          return map;
        },
      },

      /**
       * Expose the Permissions SettingsState enum to HTML bindings.
       */
      settingsStateEnum_: {
        type: Object,
        value: SettingsState,
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

      enableSafetyHub_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafetyHub') &&
              !loadTimeData.getBoolean('isGuest');
        },
      },

      // <if expr="chrome_root_store_cert_management_ui">
      enableCertManagementUIV2_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('enableCertManagementUIV2');
        },
      },
      // </if>

      enableKeyboardAndPointerLockPrompt_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableKeyboardAndPointerLockPrompt'),
      },

      enableWebAppInstallation_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebAppInstallation'),
      },

      isNotificationAllowed_: Boolean,
      isLocationAllowed_: Boolean,
      notificationPermissionsReviewHeader_: String,
      notificationPermissionsReviewSubeader_: String,
    };
  }

  private isGuest_: boolean;
  private showPersistentPermissions_: boolean;
  private showClearBrowsingDataDialog_: boolean;
  private showPrivacyGuideDialog_: boolean;
  private enableSafeBrowsingSubresourceFilter_: boolean;
  private enableBlockAutoplayContentSetting_: boolean;
  private blockAutoplayStatus_: BlockAutoplayStatus;
  private enableFederatedIdentityApiContentSetting_: boolean;
  private enablePaymentHandlerContentSetting_: boolean;
  private enableHandTrackingContentSetting_: boolean;
  private enableExperimentalWebPlatformFeatures_: boolean;
  private enableSecurityKeysSubpage_: boolean;
  private enableSmartCardReadersContentSetting_: boolean;
  private enableWebBluetoothNewPermissionsBackend_: boolean;
  private enableWebPrintingContentSetting_: boolean;
  private showNotificationPermissionsReview_: boolean;
  private isPrivacySandboxRestricted_: boolean;
  private isPrivacySandboxRestrictedNoticeEnabled_: boolean;
  private isProactiveTopicsBlockingEnabled_: boolean;
  private enableAutomaticFullscreenContentSetting_: boolean;
  private is3pcdRedesignEnabled_: boolean;
  private privateStateTokensEnabled_: boolean;
  private autoPictureInPictureEnabled_: boolean;
  private capturedSurfaceControlEnabled_: boolean;
  private enableAiSettingsPageRefresh_: boolean;
  private enableComposeProactiveNudge_: boolean;
  private enableSafetyHub_: boolean;
  private enableWebAppInstallation_: boolean;
  private focusConfig_: FocusConfig;
  private searchFilter_: string;
  private notificationPermissionsReviewHeader_: string;
  private notificationPermissionsReviewSubheader_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteSettingsPrefsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private isNotificationAllowed_: boolean;
  private isLocationAllowed_: boolean;
  // <if expr="chrome_root_store_cert_management_ui">
  private enableCertManagementUIV2_: boolean;
  // </if>
  private enableKeyboardAndPointerLockPrompt_: boolean;

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

    if (!this.isGuest_) {
      this.addWebUiListener(
          SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
          (sites: NotificationPermission[]) =>
              this.onReviewNotificationPermissionListChanged_(sites));

      this.safetyHubBrowserProxy_.getNotificationPermissionReview().then(
          (sites: NotificationPermission[]) =>
              this.onReviewNotificationPermissionListChanged_(sites));
    }

    this.updateLocationAndNotificationState_();
  }

  override currentRouteChanged() {
    this.showClearBrowsingDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.CLEAR_BROWSER_DATA;
    this.showPrivacyGuideDialog_ =
        Router.getInstance().getCurrentRoute() === routes.PRIVACY_GUIDE &&
        this.isPrivacyGuideAvailable;

    // Only record the metrics when the user navigates to the notification
    // settings page that shows the entry point.
    if (Router.getInstance().getCurrentRoute() ===
            routes.SITE_SETTINGS_NOTIFICATIONS &&
        this.showNotificationPermissionsReview_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointShown(
          SafetyHubEntryPoint.NOTIFICATIONS);
    }
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

  private onTrackingProtectionClick_() {
    this.interactedWithPage_();
    this.metricsBrowserProxy_.recordAction(
        'Settings.TrackingProtection.OpenedFromPrivacyPage');
    Router.getInstance().navigateTo(routes.TRACKING_PROTECTION);
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

  private async updateLocationAndNotificationState_() {
    const [notificationDefaultValue, locationDefaultValue] = await Promise.all([
      this.siteSettingsPrefsBrowserProxy_.getDefaultValueForContentType(
          ContentSettingsTypes.NOTIFICATIONS),
      this.siteSettingsPrefsBrowserProxy_.getDefaultValueForContentType(
          ContentSettingsTypes.GEOLOCATION),
    ]);
    this.isNotificationAllowed_ =
        (notificationDefaultValue.setting === ContentSetting.ASK);
    this.isLocationAllowed_ =
        (locationDefaultValue.setting === ContentSetting.ASK);
  }

  private onLocationTopLevelRadioChanged_(event: CustomEvent<{value: string}>) {
    const radioButtonName = event.detail.value;
    switch (radioButtonName) {
      case 'location-block-radio-button':
        this.setPrefValue('generated.geolocation', SettingsState.BLOCK);
        this.isLocationAllowed_ = false;
        break;
      case 'location-ask-radio-button':
        this.setPrefValue('generated.geolocation', SettingsState.CPSS);
        this.isLocationAllowed_ = true;
        break;
    }
  }

  private onNotificationTopLevelRadioChanged_(
      event: CustomEvent<{value: string}>) {
    const radioButtonName = event.detail.value;
    switch (radioButtonName) {
      case 'notification-block-radio-button':
        this.setPrefValue('generated.notification', SettingsState.BLOCK);
        this.isNotificationAllowed_ = false;
        break;
      case 'notification-ask-radio-button':
        this.setPrefValue('generated.notification', SettingsState.CPSS);
        this.isNotificationAllowed_ = true;
        break;
    }
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

  private async onReviewNotificationPermissionListChanged_(
      permissions: NotificationPermission[]) {
    // The notification permissions review is shown when there are items to
    // review (provided the feature is enabled and should be shown). Once
    // visible it remains that way to show completion info, even if the list is
    // emptied.
    if (this.showNotificationPermissionsReview_) {
      return;
    }
    this.showNotificationPermissionsReview_ = !this.isGuest_ &&
        permissions.length > 0;

    this.notificationPermissionsReviewHeader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsPrimaryLabel', permissions.length);
    this.notificationPermissionsReviewSubheader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsSecondaryLabel',
            permissions.length);
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
        return this.i18n('thirdPartyCookiesLinkRowSublabelDisabledIncognito');
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

  private shouldShowManageTopics_(): boolean {
    return this.isProactiveTopicsBlockingEnabled_ &&
        !this.isPrivacySandboxRestricted_;
  }

  private shouldShowComposeProactiveNudge_(): boolean {
    return this.enableComposeProactiveNudge_ &&
        !this.enableAiSettingsPageRefresh_;
  }

  private onSafetyHubButtonClick_() {
    this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
        SafetyHubEntryPoint.NOTIFICATIONS);
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page': SettingsPrivacyPageElement;
  }
}

customElements.define(
    SettingsPrivacyPageElement.is, SettingsPrivacyPageElement);
