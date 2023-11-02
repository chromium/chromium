// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-app-permission-row' is a custom row element
 * representing an app. This is used in the subpages of the OS Settings Privacy
 * controls page.
 */

import {Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermissionValue, createTriStatePermissionValue, isBoolValue, isPermissionEnabled, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {App, AppPermissionsHandlerInterface} from '../mojom-webui/app_permission_handler.mojom-webui.js';

import {getAppPermissionProvider} from './mojo_interface_provider.js';
import {getTemplate} from './privacy_hub_app_permission_row.html.js';

function getPermissionValueAsTriState(permission: Permission): TriState {
  if (isTriStateValue(permission.value)) {
    return castExists(permission.value.tristateValue);
  }
  return permission.value.boolValue ? TriState.kAllow : TriState.kBlock;
}

const SettingsPrivacyHubAppPermissionRowBase = I18nMixin(PolymerElement);

export class SettingsPrivacyHubAppPermissionRow extends
    SettingsPrivacyHubAppPermissionRowBase {
  static get is() {
    return 'settings-privacy-hub-app-permission-row' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: {
        type: Object,
      },

      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: {
        type: String,
        reflectToAttribute: true,
      },

      /**
       * Boolean state indicator for the value of the permission of type
       * `this.permissionType`.
       *
       * `TriState.kAllow` maps to `true`.
       * `TriState.kAsk` and `TriState.kBlock` maps to `false`.
       */
      checked_: {
        type: Boolean,
        value: false,
      },

      /**
       * A text describing the permission value.
       */
      permissionText_: {
        type: String,
        value: '',
      },
    };
  }

  app: App;
  permissionType: PermissionTypeIndex;
  private checked_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private permissionText_: string;

  static get observers() {
    return ['onPermissionChange_(app.permissions.*)'];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppPermissionProvider();
  }

  private onPermissionChange_(): void {
    const permission =
        castExists(this.app.permissions[PermissionType[this.permissionType]]);

    this.checked_ = isPermissionEnabled(permission.value);

    const value = getPermissionValueAsTriState(permission);

    if (value === TriState.kAllow && permission.details) {
      this.permissionText_ = this.i18n(
          'privacyHubPermissionAllowedTextWithDetails', permission.details);
      return;
    }

    switch (value) {
      case TriState.kAllow:
        this.permissionText_ = this.i18n('privacyHubPermissionAllowedText');
        break;
      case TriState.kBlock:
        this.permissionText_ = this.i18n('privacyHubPermissionDeniedText');
        break;
      case TriState.kAsk:
        this.permissionText_ = this.i18n('privacyHubPermissionAskText');
        break;
    }
  }

  private onPermissionRowClick_(): void {
    const permission =
        castExists(this.app.permissions[PermissionType[this.permissionType]]);

    if (isBoolValue(permission.value)) {
      permission.value = createBoolPermissionValue(!this.checked_);
    } else if (isTriStateValue(permission.value)) {
      permission.value = createTriStatePermissionValue(
          this.checked_ ? TriState.kBlock : TriState.kAllow);
    }

    this.mojoInterfaceProvider_.setPermission(this.app.id, permission);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubAppPermissionRow.is]: SettingsPrivacyHubAppPermissionRow;
  }
}

customElements.define(
    SettingsPrivacyHubAppPermissionRow.is, SettingsPrivacyHubAppPermissionRow);
