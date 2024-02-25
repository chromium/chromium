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

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';
import type {NotificationPermission, SafetyHubBrowserProxy} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';

import type {SettingsSafetyCheckChildElement} from './safety_check_child.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';
import {getTemplate} from './safety_check_notification_permissions.html.js';

export interface SettingsSafetyCheckNotificationPermissionsElement {
  $: {
    'safetyCheckChild': SettingsSafetyCheckChildElement,
  };
}

const SettingsSafetyCheckNotificationPermissionsElementBase =
    WebUiListenerMixin(PolymerElement);

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

      headerString_: String,
    };
  }

  private iconStatus_: SafetyCheckIconStatus;
  private headerString_: string;
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Register for review notification permission list updates.
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) => this.onSitesChanged_(sites));

    this.safetyHubBrowserProxy_.getNotificationPermissionReview().then(
        this.onSitesChanged_.bind(this));
  }

  private onButtonClick_() {
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_NOTIFICATIONS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  private async onSitesChanged_(sites: NotificationPermission[]) {
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewHeaderLabel', sites.length);
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
