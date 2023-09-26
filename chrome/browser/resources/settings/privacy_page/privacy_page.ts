// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../safety_hub/safety_hub_module.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/settings_category_default_radio_group.js';
import './privacy_guide/privacy_guide_dialog.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import {NotificationPermission, SafetyHubBrowserProxy, SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode, SettingsState} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyGuideAvailabilityMixin} from './privacy_guide/privacy_guide_availability_mixin.js';
import {getTemplate} from './privacy_page.html.js';

interface BlockAutoplayStatus {
  enabled: boolean;
  pref: chrome.settingsPrivate.PrefObject<boolean>;
}

export interface SettingsPrivacyPageElement {
  $: {
    clearBrowsingData: CrLinkRowElement,
    cookiesLinkRow: CrLinkRowElement,
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

      cookieSettingDescription_: String,

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

      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
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

      isPrivacySandboxSettings4_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxSettings4'),
      },

      is3pcdRedesignEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled'),
      },

      privateStateTokensEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privateStateTokensEnabled'),
      },

      enablePermissionStorageAccessApi_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enablePermissionStorageAccessApi'),
      },

      autoPictureInPictureEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('autoPictureInPictureEnabled'),
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

      blockMidiByDefault_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('blockMidiByDefault'),
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (routes.SECURITY) {
            map.set(routes.SECURITY.path, '#securityLinkRow');
          }

          if (routes.COOKIES) {
            const selector =
                loadTimeData.getBoolean('isPrivacySandboxSettings4') ?
                '#thirdPartyCookiesLinkRow' :
                '#cookiesLinkRow';
            map.set(`${routes.COOKIES.path}_${routes.PRIVACY.path}`, selector);
            map.set(`${routes.COOKIES.path}_${routes.BASIC.path}`, selector);
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

      safetyCheckNotificationPermissionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'safetyCheckNotificationPermissionsEnabled');
        },
      },

      notificationsDefaultBehaviorLabel_: {
        type: String,
        computed:
            'computeNotificationsDefaultBehaviorLabel_(safetyCheckNotificationPermissionsEnabled_)',
      },

      enableSafetyHub_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafetyHub') &&
              !loadTimeData.getBoolean('isGuest');
        },
      },

      showPreloadingSubpage_: {
        type: Boolean,
        value: () => !loadTimeData.getBoolean(
            'isPerformanceSettingsPreloadingSubpageEnabled'),
      },

      showDedicatedCpssSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('permissionDedicatedCpssSettings');
        },
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
  private cookieSettingDescription_: string;
  private enableBlockAutoplayContentSetting_: boolean;
  private blockAutoplayStatus_: BlockAutoplayStatus;
  private blockMidiByDefault_: boolean;
  private enableFederatedIdentityApiContentSetting_: boolean;
  private enablePaymentHandlerContentSetting_: boolean;
  private enableExperimentalWebPlatformFeatures_: boolean;
  private enableSecurityKeysSubpage_: boolean;
  private enableWebBluetoothNewPermissionsBackend_: boolean;
  private showNotificationPermissionsReview_: boolean;
  private isPrivacySandboxRestricted_: boolean;
  private isPrivacySandboxRestrictedNoticeEnabled_: boolean;
  private isPrivacySandboxSettings4_: boolean;
  private is3pcdRedesignEnabled_: boolean;
  private privateStateTokensEnabled_: boolean;
  private autoPictureInPictureEnabled_: boolean;
  private safetyCheckNotificationPermissionsEnabled_: boolean;
  private enablePermissionStorageAccessApi_: boolean;
  private enableSafetyHub_: boolean;
  private showPreloadingSubpage_: boolean;
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
  private showDedicatedCpssSetting_: boolean;

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

    this.siteSettingsPrefsBrowserProxy_.getCookieSettingDescription().then(
        (description: string) => this.cookieSettingDescription_ = description);

    this.addWebUiListener(
        'cookieSettingDescriptionChanged',
        (description: string) => this.cookieSettingDescription_ = description);

    if (this.safetyCheckNotificationPermissionsEnabled_ && !this.isGuest_) {
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

    if (this.isPrivacySandboxSettings4_) {
      Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX);
      return;
    }

    // Create a MouseEvent directly to avoid Polymer failing to synthesise a
    // click event if this function was called in response to a touch event.
    // See crbug.com/1253883 for details.
    // TODO(crbug/1159942): Replace this with an ordinary OpenWindowProxy call.
    this.shadowRoot!.querySelector<HTMLAnchorElement>('#privacySandboxLink')!
        .dispatchEvent(new MouseEvent('click'));
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

  private onLocationAskClicked_() {
    this.isLocationAllowed_ = true;
    this.setPrefValue('generated.geolocation', SettingsState.CPSS);
  }

  private onNotificationAskClicked_() {
    this.isNotificationAllowed_ = true;
    this.setPrefValue('generated.notification', SettingsState.CPSS);
  }

  private onLocationBlockClicked_() {
    this.isLocationAllowed_ = false;
  }

  private onNotificationBlockClicked_() {
    this.isNotificationAllowed_ = false;
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
        this.safetyCheckNotificationPermissionsEnabled_ &&
        permissions.length > 0;

    this.notificationPermissionsReviewHeader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel',
            permissions.length);
    this.notificationPermissionsReviewSubheader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewSecondaryLabel',
            permissions.length);
  }

  private interactedWithPage_() {
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.USED_PRIVACY_CARD);
  }

  private computePrivacySandboxSublabel_(): string {
    const enabled = this.getPref('privacy_sandbox.apis_enabled_v2').value;
    return enabled ? this.i18n('privacySandboxTrialsEnabled') :
                     this.i18n('privacySandboxTrialsDisabled');
  }

  private computeAdPrivacySublabel_(): string {
    // When the privacy sandbox is restricted with a notice, the sublabel
    // wording indicates measurement only, rather than general ad privacy.
    const restricted = this.isPrivacySandboxRestricted_ &&
        this.isPrivacySandboxRestrictedNoticeEnabled_;
    return restricted ? this.i18n('adPrivacyRestrictedLinkRowSubLabel') :
                        this.i18n('adPrivacyLinkRowSubLabel');
  }

  private computeNotificationsDefaultBehaviorLabel_(): string {
    return this.safetyCheckNotificationPermissionsEnabled_ ?
        this.i18n('siteSettingsNotificationsDefaultBehaviorDescription') :
        this.i18n('siteSettingsDefaultBehaviorDescription');
  }

  private computeThirdPartyCookiesSublabel_(): string {
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

  private isPrivacySandboxSettings3Enabled_(): boolean {
    return !this.isPrivacySandboxRestricted_ &&
        !this.isPrivacySandboxSettings4_;
  }

  private isPrivacySandboxSettings4Enabled_(): boolean {
    return (!this.isPrivacySandboxRestricted_ ||
            this.isPrivacySandboxRestrictedNoticeEnabled_) &&
        this.isPrivacySandboxSettings4_;
  }

  private isPrivacySandboxSettings4CookiesPageEnabled_(): boolean {
    return this.isPrivacySandboxSettings4_ && !this.is3pcdRedesignEnabled_;
  }

  private isPrivacySandboxSettings3CookiesPageEnabled_(): boolean {
    return !this.isPrivacySandboxSettings4_ && !this.is3pcdRedesignEnabled_;
  }

  private onSafetyHubButtonClick_() {
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
