// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-app-permission-row' is a custom row element
 * representing an app. This is used in the subpages of the OS Settings Privacy
 * controls page.
 */

import {AppType, Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermissionValue, createTriStatePermissionValue, isBoolValue, isPermissionEnabled, isTriStateValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {App, AppPermissionsHandlerInterface} from '../mojom-webui/app_permission_handler.mojom-webui.js';

import {getAppPermissionProvider} from './mojo_interface_provider.js';
import {getTemplate} from './privacy_hub_app_permission_row.html.js';
import {NUMBER_OF_POSSIBLE_USER_ACTIONS, PrivacyHubSensorSubpageUserAction} from './privacy_hub_metrics_util.js';

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

      isPermissionManaged_: {
        type: Boolean,
        value: false,
      },

      shouldRedirectToAndroidSettings_: {
        type: Boolean,
        computed: 'computeShouldRedirectToAndroidSettings_(app.type, ' +
            'isPermissionManaged_)',
      },

      shouldDisableToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableToggle_(isPermissionManaged_, ' +
            'shouldRedirectToAndroidSettings_)',
      },

      ariaDescription_: {
        type: String,
        computed: 'computeAriaDescription_(permissionText_)',
      },

      androidSettingsLinkAriaDescription_: {
        type: String,
        computed: 'computeAndroidSettingsLinkAriaDescription_(permissionText_)',
      },
    };
  }

  app: App;
  permissionType: PermissionTypeIndex;
  private androidSettingsLinkAriaDescription_: string;
  private ariaDescription_: string;
  private checked_: boolean;
  private isPermissionManaged_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private permissionText_: string;
  private shouldDisableToggle_: boolean;
  private shouldRedirectToAndroidSettings_: boolean;

  static get observers() {
    return ['onPermissionChange_(app.permissions.*)'];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider_ = getAppPermissionProvider();
  }

  override ready(): void {
    super.ready();
    this.addEventListener('click', this.onPermissionRowClick_.bind(this));
  }


  private onPermissionChange_(): void {
    const permission =
        castExists(this.app.permissions[PermissionType[this.permissionType]]);

    this.checked_ = isPermissionEnabled(permission.value);

    this.isPermissionManaged_ = permission.isManaged;

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

  private getUserActionHistogramName(): string {
    return `ChromeOS.PrivacyHub.${
        this.permissionType.substring(1)}Subpage.UserAction`;
  }

  private togglePermissionState_(): void {
    const permission =
        castExists(this.app.permissions[PermissionType[this.permissionType]]);
    const permissionEnabled = isPermissionEnabled(permission.value);

    if (isBoolValue(permission.value)) {
      permission.value = createBoolPermissionValue(!permissionEnabled);
    } else if (isTriStateValue(permission.value)) {
      permission.value = createTriStatePermissionValue(
          permissionEnabled ? TriState.kBlock : TriState.kAllow);
    }

    this.mojoInterfaceProvider_.setPermission(this.app.id, permission);

    chrome.metricsPrivate.recordEnumerationValue(
        this.getUserActionHistogramName(),
        PrivacyHubSensorSubpageUserAction.APP_PERMISSION_CHANGED,
        NUMBER_OF_POSSIBLE_USER_ACTIONS);
  }

  private onPermissionRowClick_(): void {
    if (this.isPermissionManaged_) {
      return;
    }

    if (this.shouldRedirectToAndroidSettings_) {
      this.mojoInterfaceProvider_.openNativeSettings(this.app.id);

      chrome.metricsPrivate.recordEnumerationValue(
          this.getUserActionHistogramName(),
          PrivacyHubSensorSubpageUserAction.ANDROID_SETTINGS_LINK_CLICKED,
          Object.keys(PrivacyHubSensorSubpageUserAction).length);
      return;
    }

    this.togglePermissionState_();
  }

  private onToggleClick_(e: Event): void {
    e.stopImmediatePropagation();
    e.preventDefault();

    this.togglePermissionState_();
  }

  private onKeyup_(e: KeyboardEvent): void {
    if (e.key !== ' ') {
      return;
    }

    e.stopImmediatePropagation();
    e.preventDefault();

    this.togglePermissionState_();
  }

  private onKeydown_(e: KeyboardEvent): void {
    if (e.key !== 'Enter') {
      return;
    }

    e.stopImmediatePropagation();
    e.preventDefault();
    if (e.repeat) {
      return;
    }

    this.togglePermissionState_();
  }

  private computeShouldRedirectToAndroidSettings_(): boolean {
    return !this.isPermissionManaged_ &&
        loadTimeData.getBoolean('isArcReadOnlyPermissionsEnabled') &&
        this.app.type === AppType.kArc;
  }

  private computeShouldDisableToggle_(): boolean {
    return this.isPermissionManaged_ || this.shouldRedirectToAndroidSettings_;
  }

  private getAriaLabel_(): string {
    switch (PermissionType[this.permissionType]) {
      case PermissionType.kCamera:
        return this.i18n(
            'privacyHubCameraAppPermissionRowAriaLabel', this.app.name);
      case PermissionType.kLocation:
        return this.i18n(
            'privacyHubLocationAppPermissionRowAriaLabel', this.app.name);
      case PermissionType.kMicrophone:
        return this.i18n(
            'privacyHubMicrophoneAppPermissionRowAriaLabel', this.app.name);
      default:
        return '';
    }
  }

  private computeAriaDescription_(): string {
    return this.i18n(
        'privacyHubAppPermissionRowAriaDescription', this.permissionText_);
  }

  private computeAndroidSettingsLinkAriaDescription_(): string {
    return this.i18n(
        'privacyHubAppPermissionRowAndroidSettingsLinkAriaDescription',
        this.permissionText_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubAppPermissionRow.is]: SettingsPrivacyHubAppPermissionRow;
  }
}

customElements.define(
    SettingsPrivacyHubAppPermissionRow.is, SettingsPrivacyHubAppPermissionRow);
