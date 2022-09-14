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

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckIconStatus, SettingsSafetyCheckChildElement} from './safety_check_child.js';
import {getTemplate} from './safety_check_notification_permissions.html.js';

export interface SettingsSafetyCheckNotificationPermissionsElement {
  $: {
    'safetyCheckChild': SettingsSafetyCheckChildElement,
  };
}

export class SettingsSafetyCheckNotificationPermissionsElement extends
    PolymerElement {
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
    };
  }

  private iconStatus_: SafetyCheckIconStatus;

  private onButtonClick_() {
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_NOTIFICATIONS, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
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
