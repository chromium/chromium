// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-notification-row' is a custom row element for the OS Settings
 * Notifications Subpage. Each row contains an app icon, app name, and toggle.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermissionValue, createTriStatePermissionValue, isBoolValue, isPermissionEnabled, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../../metrics_recorder.js';
import {App, AppNotificationsHandlerInterface} from '../../mojom-webui/app_notification_handler.mojom-webui.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './app_notification_row.html.js';
import {getAppNotificationProvider} from './mojo_interface_provider.js';

export class AppNotificationRowElement extends PolymerElement {
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
  private mojoInterfaceProvider_: AppNotificationsHandlerInterface =
      getAppNotificationProvider();

  static get observers() {
    return ['updateToggleState_(app.notificationPermission.*)'];
  }

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onToggleChangeByUser_.bind(this));
  }

  private updateToggleState_(): void {
    this.checked_ = isPermissionEnabled(this.app.notificationPermission.value);
  }

  /**
   * Called when a user toggles the notification on/off via click or keypress.
   */
  private onToggleChangeByUser_(): void {
    const permission = this.app.notificationPermission;
    if (permission.isManaged) {
      return;
    }

    if (isBoolValue(permission.value)) {
      permission.value = createBoolPermissionValue(!this.checked_);
    } else if (isTriStateValue(permission.value)) {
      permission.value = createTriStatePermissionValue(
          this.checked_ ? TriState.kBlock : TriState.kAllow);
    }

    this.mojoInterfaceProvider_.setNotificationPermission(
        this.app.id, permission);
    recordSettingChange(
        Setting.kAppNotificationOnOff, {boolValue: !this.checked_});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-notification-row': AppNotificationRowElement;
  }
}

customElements.define(AppNotificationRowElement.is, AppNotificationRowElement);
