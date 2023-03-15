// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../site_settings/settings_category_default_radio_group.js';
import './privacy_guide/privacy_guide_dialog.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {FocusConfig} from '../focus_config.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideInteractions} from '../metrics_browser_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import {ChooserType, ContentSettingsTypes, CookieControlsMode, NotificationSetting} from '../site_settings/constants.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {PrivacyGuideAvailabilityMixin} from './privacy_guide/privacy_guide_availability_mixin.js';
import {getTemplate} from './privacy_page.html.js';
import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.js';

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

      enableQuietNotificationPromptsSetting_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableQuietNotificationPromptsSetting'),
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

      isPrivacySandboxSettings4_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isPrivacySandboxSettings4'),
      },

      privateStateTokensEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('privateStateTokensEnabled'),
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
                '#cookiesLinkRow');
            map.set(
                `${routes.COOKIES.path}_${routes.BASIC.path}`,
                '#cookiesLinkRow');
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
       * Expose NotificationSetting enum to HTML bindings.
       */
      notificationSettingEnum_: {
        type: Object,
        value: NotificationSetting,
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
  private enableFederatedIdentityApiContentSetting_: boolean;
  private enablePaymentHandlerContentSetting_: boolean;
  private enableExperimentalWebPlatformFeatures_: boolean;
  private enableSecurityKeysSubpage_: boolean;
  private enableQuietNotificationPromptsSetting_: boolean;
  private enableWebBluetoothNewPermissionsBackend_: boolean;
  private showNotificationPermissionsReview_: boolean;
  private isPrivacySandboxRestricted_: boolean;
  private isPrivacySandboxSettings4_: boolean;
  private privateStateTokensEnabled_: boolean;
  private safetyCheckNotificationPermissionsEnabled_: boolean;
  private focusConfig_: FocusConfig;
  private searchFilter_: string;
  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteSettingsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

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

    this.siteSettingsBrowserProxy_.getCookieSettingDescription().then(
        (description: string) => this.cookieSettingDescription_ = description);

    this.addWebUiListener(
        'cookieSettingDescriptionChanged',
        (description: string) => this.cookieSettingDescription_ = description);

    this.addWebUiListener(
        'notification-permission-review-list-maybe-changed',
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.siteSettingsBrowserProxy_.getNotificationPermissionReview().then(
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));
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

  private onClearBrowsingDataTap_() {
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

  private onPrivacyGuideClick_() {
    this.metricsBrowserProxy_.recordPrivacyGuideEntryExitHistogram(
        PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY);
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.StartPrivacySettings');
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE, /* dynamicParams */ undefined,
        /* removeSearch */ true);
  }

  private onReviewNotificationPermissionListChanged_(
      permissions: NotificationPermission[]) {
    // The notification permissions review is shown when there are items to
    // review (provided the feature is enabled). Once visible it remains that
    // way to show completion info, even if the list is emptied.
    if (this.showNotificationPermissionsReview_) {
      return;
    }
    this.showNotificationPermissionsReview_ =
        this.safetyCheckNotificationPermissionsEnabled_ &&
        permissions.length > 0;
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
    return !this.isPrivacySandboxRestricted_ && this.isPrivacySandboxSettings4_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page': SettingsPrivacyPageElement;
  }
}

customElements.define(
    SettingsPrivacyPageElement.is, SettingsPrivacyPageElement);
