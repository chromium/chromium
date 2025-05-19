// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-notifications-subpage' contains the settings for notifications
 * under Site Settings.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import './site_settings_shared.css.js';
import '../privacy_icons.html.js';
import '../privacy_page/collapse_radio_button.js';
import '../safety_hub/safety_hub_module.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubEntryPoint} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import type {NotificationPermission, SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';

import {ContentSetting, ContentSettingsTypes, SettingsState} from './constants.js';
import {getTemplate} from './notifications_page.html.js';
import type {SiteSettingsPrefsBrowserProxy} from './site_settings_prefs_browser_proxy.js';
import {SiteSettingsPrefsBrowserProxyImpl} from './site_settings_prefs_browser_proxy.js';

const NotificationsPageElementBase =
    RouteObserverMixin(WebUiListenerMixin(PrefsMixin(PolymerElement)));

export class NotificationsPageElement extends NotificationsPageElementBase {
  static get is() {
    return 'settings-notifications-page';
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

      enablePermissionSiteSettingsRadioButton_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enablePermissionSiteSettingsRadioButton'),
      },

      /** Expose the Permissions SettingsState enum to HTML bindings. */
      settingsStateEnum_: {
        type: Object,
        value: SettingsState,
      },

      /** Expose ContentSettingsTypes enum to HTML bindings. */
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      showNotificationPermissionsReview_: {
        type: Boolean,
        value: false,
      },

      /** Expose ContentSetting enum to HTML bindings. */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      shouldShowSafetyHub_: {
        type: Boolean,
        value() {
          return !loadTimeData.getBoolean('isGuest');
        },
      },

      isNotificationAllowed_: Boolean,
      notificationPermissionsReviewHeader_: String,
      notificationPermissionsReviewSubheader_: String,
    };
  }

  declare private isGuest_: boolean;
  declare private enablePermissionSiteSettingsRadioButton_: boolean;
  declare private shouldShowSafetyHub_: boolean;
  declare private isNotificationAllowed_: boolean;
  declare private showNotificationPermissionsReview_: boolean;
  declare private notificationPermissionsReviewHeader_: string;
  declare private notificationPermissionsReviewSubheader_: string;
  private siteSettingsPrefsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.updateNotificationState_();

    if (this.isGuest_) {
      return;
    }

    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.safetyHubBrowserProxy_.getNotificationPermissionReview().then(
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));
  }


  override currentRouteChanged() {
    // Only record the metrics when the user navigates to the notification
    // settings page that shows the entry point.
    if (this.showNotificationPermissionsReview_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointShown(
          SafetyHubEntryPoint.NOTIFICATIONS);
    }
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
    this.showNotificationPermissionsReview_ =
        !this.isGuest_ && permissions.length > 0;

    this.notificationPermissionsReviewHeader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsPrimaryLabel', permissions.length);
    this.notificationPermissionsReviewSubheader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyHubNotificationPermissionsSecondaryLabel',
            permissions.length);
  }

  private async updateNotificationState_() {
    const [notificationDefaultValue] = await Promise.all([
      this.siteSettingsPrefsBrowserProxy_.getDefaultValueForContentType(
          ContentSettingsTypes.NOTIFICATIONS),
    ]);
    this.isNotificationAllowed_ =
        (notificationDefaultValue.setting === ContentSetting.ASK);
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

  private onNotificationTopLevelRadioChanged2_(
      event: CustomEvent<{value: boolean}>) {
    const selected = event.detail.value;
    if (selected) {
      this.setPrefValue('generated.notification', SettingsState.CPSS);
      this.isNotificationAllowed_ = true;
    } else {
      this.setPrefValue('generated.notification', SettingsState.BLOCK);
      this.isNotificationAllowed_ = false;
    }
  }

  private onSafetyHubButtonClick_() {
    this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
        SafetyHubEntryPoint.NOTIFICATIONS);
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-notifications-page': NotificationsPageElement;
  }
}

customElements.define(NotificationsPageElement.is, NotificationsPageElement);
