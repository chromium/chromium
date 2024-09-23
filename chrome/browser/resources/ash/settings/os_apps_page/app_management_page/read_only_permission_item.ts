// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '../../os_privacy_page/privacy_hub_allow_sensor_access_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {getPermission} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MediaDevicesProxy} from '../../common/media_devices_proxy.js';

import {PrivacyHubMixin} from './privacy_hub_mixin.js';
import {getTemplate} from './read_only_permission_item.html.js';
import {getPermissionDescriptionString, isSensorAvailable} from './util.js';

const AppManagementReadOnlyPermissionItemElementBase =
    PrivacyHubMixin(PrefsMixin(I18nMixin(PolymerElement)));

export class AppManagementReadOnlyPermissionItemElement extends
    AppManagementReadOnlyPermissionItemElementBase {
  static get is() {
    return 'app-management-read-only-permission-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The name of the permission, to be displayed to the user.
       */
      permissionLabel: String,

      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: {
        type: String,
        reflectToAttribute: true,
      },

      icon: String,

      app: Object,

      /**
       * True if the permission type is available for the app.
       */
      available_: {
        type: Boolean,
        computed: 'computeAvailable_(app, permissionType)',
        reflectToAttribute: true,
      },

      showAllowSensorAccessDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether sensor relevant to the `permissionType` is available.
       */
      sensorAvailable_: {
        type: Boolean,
        value: true,
      },
    };
  }

  app: App;
  permissionLabel: string;
  permissionType: PermissionTypeIndex;
  icon: string;
  private available_: boolean;
  private sensorAvailable_: boolean;
  private showAllowSensorAccessDialog_: boolean;

  override ready(): void {
    super.ready();

    this.updateSensorAvailability_();
    MediaDevicesProxy.getMediaDevices().addEventListener('devicechange', () => {
      this.updateSensorAvailability_();
    });
  }

  private async updateSensorAvailability_(): Promise<void> {
    this.sensorAvailable_ = await isSensorAvailable(this.permissionType);
  }

  private computeAvailable_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    return getPermission(app, permissionType) !== undefined;
  }

  private getPermissionDescriptionString_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): string {
    const isSensorBlocked =
        loadTimeData.getBoolean('privacyHubAppPermissionsV2Enabled') &&
        this.isSensorBlocked(permissionType);
    return getPermissionDescriptionString(
        app, permissionType, this.sensorAvailable_, isSensorBlocked,
        this.microphoneHardwareToggleActive,
        this.microphoneMutedBySecurityCurtain, this.cameraSwitchForceDisabled);
  }

  private isManaged_(
      app: App|undefined,
      permissionType: PermissionTypeIndex|undefined): boolean {
    if (app === undefined || permissionType === undefined) {
      return false;
    }

    const permission = getPermission(app, permissionType);
    assert(permission);

    return permission.isManaged;
  }

  private launchAllowSensorAccessDialog_(e: CustomEvent): void {
    e.detail.event.preventDefault();
    e.stopPropagation();

    this.showAllowSensorAccessDialog_ = true;
  }

  private onAllowSensorAccessDialogClose_(): void {
    this.showAllowSensorAccessDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppManagementReadOnlyPermissionItemElement.is]:
        AppManagementReadOnlyPermissionItemElement;
  }
}

customElements.define(
    AppManagementReadOnlyPermissionItemElement.is,
    AppManagementReadOnlyPermissionItemElement);
