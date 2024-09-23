// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog asks users to enable system wide camera, microphone
 * or location access.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {assertNotReached} from '//resources/js/assert.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './privacy_hub_allow_sensor_access_dialog.html.js';
import {GeolocationAccessLevel} from './privacy_hub_geolocation_subpage.js';

const PrivacyHubAllowSensorAccessDialogBase =
    PrefsMixin(I18nMixin(PolymerElement));

class PrivacyHubAllowSensorAccessDialog extends
    PrivacyHubAllowSensorAccessDialogBase {
  static get is() {
    return 'settings-privacy-hub-allow-sensor-access-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * A string version of the permission type. Must be a value of the
       * permission type enum in appManagement.mojom.PermissionType.
       */
      permissionType: {
        type: String,
        reflectToAttribute: true,
      },

      title_: {
        type: String,
        computed: 'computeTitle_(permissionType)',
      },

      body_: {
        type: String,
        computed: 'computeBody_(permissionType)',
      },
    };
  }

  permissionType: PermissionTypeIndex;
  private body_: string;
  private title_: string;

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onClick_.bind(this));
  }

  private onClick_(e: Event): void {
    e.stopPropagation();
  }

  private computeBody_(): string {
    switch (PermissionType[this.permissionType]) {
      case PermissionType.kCamera:
        return this.i18n('privacyHubAllowCameraAccessDialogBodyText');
      case PermissionType.kLocation:
        return this.i18n('privacyHubAllowLocationAccessDialogBodyText');
      case PermissionType.kMicrophone:
        return this.i18n('privacyHubAllowMicrophoneAccessDialogBodyText');
      default:
        assertNotReached();
    }
  }

  private computeTitle_(): string {
    switch (PermissionType[this.permissionType]) {
      case PermissionType.kCamera:
        return this.i18n('privacyHubAllowCameraAccessDialogTitle');
      case PermissionType.kLocation:
        return this.i18n('privacyHubAllowLocationAccessDialogTitle');
      case PermissionType.kMicrophone:
        return this.i18n('privacyHubAllowMicrophoneAccessDialogTitle');
      default:
        assertNotReached();
    }
  }

  private onConfirmButtonClick_(): void {
    this.getDialog_().close();
    switch (PermissionType[this.permissionType]) {
      case PermissionType.kCamera:
        this.setPrefValue('ash.user.camera_allowed', true);
        return;
      case PermissionType.kLocation:
        this.setPrefValue(
            'ash.user.geolocation_access_level',
            GeolocationAccessLevel.ALLOWED);
        return;
      case PermissionType.kMicrophone:
        this.setPrefValue('ash.user.microphone_allowed', true);
        return;
      default:
        assertNotReached();
    }
  }

  private onCancelButtonClick_(): void {
    this.getDialog_().close();
  }

  private getDialog_(): CrDialogElement {
    return castExists(this.shadowRoot!.querySelector<CrDialogElement>(
        '#allowSensorAccessDialog'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrivacyHubAllowSensorAccessDialog.is]: PrivacyHubAllowSensorAccessDialog;
  }
}

customElements.define(
    PrivacyHubAllowSensorAccessDialog.is, PrivacyHubAllowSensorAccessDialog);
