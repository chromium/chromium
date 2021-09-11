// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/app-management/file_path.mojom-lite.js';
import '/app-management/image.mojom-lite.js';
import '/app-management/types.mojom-lite.js';
import '/os_apps_page/app_notification_handler.mojom-lite.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../../metrics_recorder.m.js';

import {getAppNotificationProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'app-notification-row' is a custom row element for the OS Settings
 * Notifications Subpage. Each row contains an app icon, app name, and toggle.
 */
export class AppNotificationRowElement extends PolymerElement {
  static get is() {
    return 'app-notification-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Object} */
      app: {
        type: Object,
      },

      /** @type {!boolean} */
      checked_: {
        type: Boolean,
        value: false,
      }
    };
  }

  static get observers() {
    return ['isNotificationPermissionEnabled_(app.notificationPermission.*)'];
  }

  constructor() {
    super();

    /** @private */
    this.mojoInterfaceProvider_ = getAppNotificationProvider();
  }

  isNotificationPermissionEnabled_() {
    if (this.app.notificationPermission.valueType ===
            apps.mojom.PermissionValueType.kBool &&
        this.app.notificationPermission.value === 1) {
      this.checked_ = true;
      return;
    }

    if (this.app.notificationPermission.valueType ===
            apps.mojom.PermissionValueType.kTriState &&
        this.app.notificationPermission.value === apps.mojom.TriState.kAllow) {
      this.checked_ = true;
      return;
    }

    this.checked_ = false;
  }

  /** @param {!Event} event */
  onNotificationRowClicked_(event) {
    const permission = this.app.notificationPermission;

    if (permission.valueType === apps.mojom.PermissionValueType.kBool) {
      // apps.mojom.permission.value expects a number type.
      permission.value = this.checked_ ? 0 : 1;
    } else if (
        permission.valueType === apps.mojom.PermissionValueType.kTriState) {
      permission.value = this.checked_ ? apps.mojom.TriState.kBlock :
                                         apps.mojom.TriState.kAllow;
    }

    this.mojoInterfaceProvider_.setNotificationPermission(
        this.app.id, permission);
    recordSettingChange();
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.NotificationPage.PermissionOnOff', !this.checked_);
  }
}

customElements.define(AppNotificationRowElement.is, AppNotificationRowElement);