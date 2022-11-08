// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-notification-permissions' is the settings page containing
 * the safety check notification permissions module showing the sites that sends
 * high volume of notifications.
 */

import './safety_check_child.js';

import {WebUIListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

import {SafetyCheckIconStatus, SettingsSafetyCheckChildElement} from './safety_check_child.js';
import {getTemplate} from './safety_check_notification_permissions.html.js';

export interface SettingsSafetyCheckNotificationPermissionsElement {
  $: {
    'safetyCheckChild': SettingsSafetyCheckChildElement,
  };
}

const SettingsSafetyCheckNotificationPermissionsElementBase =
    WebUIListenerMixin(PolymerElement);

export class SettingsSafetyCheckNotificationPermissionsElement extends
    SettingsSafetyCheckNotificationPermissionsElementBase {
  static get is() {
    return 'settings-safety-check-notification-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      iconStatus_: {
        type: SafetyCheckIconStatus,
        value() {
          return SafetyCheckIconStatus.NOTIFICATION_PERMISSIONS;
        },
      },

      sites_: {
        type: Array,
        observer: 'onSitesChanged_',
      },

      headerString_: String,

      buttonAriaLabel_: String,
    };
  }

  private iconStatus_: SafetyCheckIconStatus;
  private headerString_: string;
  private buttonAriaLabel_: string;
  private sites_: NotificationPermission[] = [];
  private siteSettingsBrowserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();

    // Register for review notification permission list updates.
    this.addWebUIListener(
        'notification-permission-review-list-maybe-changed',
        (sites: NotificationPermission[]) =>
            this.onReviewNotificationPermissionListChanged_(sites));

    this.sites_ =
        await this.siteSettingsBrowserProxy_.getNotificationPermissionReview();
  }

  private onButtonClick_() {
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_NOTIFICATIONS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  private async onReviewNotificationPermissionListChanged_(
      sites: NotificationPermission[]) {
    this.sites_ = sites;
  }

  private async onSitesChanged_() {
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewHeaderLabel',
            this.sites_.length);
    this.buttonAriaLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel',
            this.sites_!.length);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-notification-permissions':
        SettingsSafetyCheckNotificationPermissionsElement;
  }
}

customElements.define(
    SettingsSafetyCheckNotificationPermissionsElement.is,
    SettingsSafetyCheckNotificationPermissionsElement);
