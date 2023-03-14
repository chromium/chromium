// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-notification-row' is a custom row element for the OS Settings
 * Notifications Subpage. Each row contains an app icon, app name, and toggle.
 */

import {TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermissionValue, createTriStatePermissionValue, isBoolValue, isPermissionEnabled, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../../metrics_recorder.js';
import {App, AppNotificationsHandlerInterface} from '../../mojom-webui/app_notification_handler.mojom-webui.js';

import {getTemplate} from './app_notification_row.html.js';
import {getAppNotificationProvider} from './mojo_interface_provider.js';

class AppNotificationRowElement extends PolymerElement {
  static get is() {
    return 'app-notification-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: {
        type: Object,
      },

      checked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  app: App;
  private checked_: boolean;
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface;

  static get observers() {
    return ['isNotificationPermissionEnabled_(app.notificationPermission.*)'];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppNotificationProvider();
  }

  private isNotificationPermissionEnabled_(): void {
    this.checked_ = isPermissionEnabled(this.app.notificationPermission.value);
  }

  private onNotificationRowClicked_(): void {
    const permission = this.app.notificationPermission;

    if (isBoolValue(permission.value)) {
      permission.value =
          createBoolPermissionValue(this.checked_ ? false : true);
    } else if (isTriStateValue(permission.value)) {
      permission.value = createTriStatePermissionValue(
          this.checked_ ? TriState.kBlock : TriState.kAllow);
    }

    this.mojoInterfaceProvider_.setNotificationPermission(
        this.app.id, permission);
    recordSettingChange();
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.NotificationPage.PermissionOnOff', !this.checked_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-notification-row': AppNotificationRowElement;
  }
}

customElements.define(AppNotificationRowElement.is, AppNotificationRowElement);
