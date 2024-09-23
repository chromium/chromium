// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-camera-subpage' contains a detailed overview about the
 * state of the system camera access.
 */

import './privacy_hub_app_permission_row.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {isPermissionEnabled} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {MediaDevicesProxy} from '../common/media_devices_proxy.js';
import {App, AppPermissionsHandlerInterface, AppPermissionsObserverReceiver} from '../mojom-webui/app_permission_handler.mojom-webui.js';

import {getAppPermissionProvider} from './mojo_interface_provider.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {getTemplate} from './privacy_hub_camera_subpage.html.js';
import {CAMERA_SUBPAGE_USER_ACTION_HISTOGRAM_NAME, NUMBER_OF_POSSIBLE_USER_ACTIONS, PrivacyHubSensorSubpageUserAction} from './privacy_hub_metrics_util.js';

/**
 * Whether the app has camera permission defined.
 */
function hasCameraPermission(app: App): boolean {
  return app.permissions[PermissionType.kCamera] !== undefined;
}

const SettingsPrivacyHubCameraSubpageBase =
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsPrivacyHubCameraSubpage extends
    SettingsPrivacyHubCameraSubpageBase {
  static get is() {
    return 'settings-privacy-hub-camera-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Apps with camera permission defined.
       * Only contains apps that are displayed in the App Management page.
       * Does not contain system apps.
       */
      appList_: {
        type: Array,
        value: [],
      },

      systemApps_: {
        type: Array,
        value: [],
      },

      connectedCameraNames_: {
        type: Array,
        value: [],
      },

      isCameraListEmpty_: {
        type: Boolean,
        computed: 'computeIsCameraListEmpty_(connectedCameraNames_)',
      },

      /**
       * Tracks if the Chrome code wants the camera switch to be disabled.
       */
      cameraSwitchForceDisabled_: {
        type: Boolean,
        value: false,
      },

      shouldDisableCameraToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableCameraToggle_(isCameraListEmpty_, ' +
            'cameraSwitchForceDisabled_)',
      },

      cameraFallbackMechanismEnabled_: {
        type: Boolean,
        value: false,
      },

      cameraAccessStateText_: {
        type: String,
        computed: 'computeCameraAccessStateText_(' +
            'cameraFallbackMechanismEnabled_, prefs.ash.user.camera_allowed.*)',
      },
    };
  }

  private appList_: App[];
  private appPermissionsObserverReceiver_: AppPermissionsObserverReceiver|null;
  private browserProxy_: PrivacyHubBrowserProxy;
  private cameraAccessStateText_: string;
  private cameraFallbackMechanismEnabled_: boolean;
  private cameraSwitchForceDisabled_: boolean;
  private connectedCameraNames_: string[];
  private isCameraListEmpty_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private shouldDisableCameraToggle_: boolean;
  private systemApps_: App[];

  constructor() {
    super();

    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();

    this.mojoInterfaceProvider_ = getAppPermissionProvider();

    this.appPermissionsObserverReceiver_ = null;
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'force-disable-camera-switch', (disabled: boolean) => {
          this.cameraSwitchForceDisabled_ = disabled;
        });
    this.browserProxy_.getInitialCameraSwitchForceDisabledState().then(
        (disabled) => {
          this.cameraSwitchForceDisabled_ = disabled;
        });
    this.browserProxy_.getCameraLedFallbackState().then((enabled) => {
      this.cameraFallbackMechanismEnabled_ = enabled;
    });

    this.updateCameraList_();
    MediaDevicesProxy.getMediaDevices().addEventListener(
        'devicechange', () => this.updateCameraList_());
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.appPermissionsObserverReceiver_ =
        new AppPermissionsObserverReceiver(this);
    this.mojoInterfaceProvider_.addObserver(
        this.appPermissionsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.updateAppLists_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appPermissionsObserverReceiver_!.$.close();
  }

  private async updateAppLists_(): Promise<void> {
    const apps = (await this.mojoInterfaceProvider_.getApps()).apps;
    this.appList_ = apps.filter(hasCameraPermission);

    this.systemApps_ =
        (await this.mojoInterfaceProvider_.getSystemAppsThatUseCamera()).apps;
  }

  private isCameraAllowed_(): boolean {
    return this.getPref('ash.user.camera_allowed').value;
  }

  private getSystemServicesPermissionText_(): string {
    return this.isCameraAllowed_() ?
        this.i18n('privacyHubSystemServicesAllowedText') :
        this.i18n('privacyHubSystemServicesBlockedText');
  }

  /**
   * The function is used for sorting app names alphabetically.
   */
  private alphabeticalSort_(first: App, second: App): number {
    return first.name!.localeCompare(second.name!);
  }

  private isCameraPermissionEnabled_(app: App): boolean {
    const permission = castExists(app.permissions[PermissionType.kCamera]);
    return isPermissionEnabled(permission.value);
  }

  /** Implements AppPermissionsObserver.OnAppUpdated */
  onAppUpdated(updatedApp: App): void {
    if (!hasCameraPermission(updatedApp)) {
      return;
    }
    const idx = this.appList_.findIndex(app => app.id === updatedApp.id);
    if (idx === -1) {
      // New app installed.
      this.push('appList_', updatedApp);
    } else {
      // An already installed app is updated.
      this.splice('appList_', idx, 1, updatedApp);
    }
  }

  /** Implements AppPermissionsObserver.OnAppRemoved */
  onAppRemoved(appId: string): void {
    const idx = this.appList_.findIndex(app => app.id === appId);
    if (idx !== -1) {
      this.splice('appList_', idx, 1);
    }
  }

  private async updateCameraList_(): Promise<void> {
    const connectedCameraNames: string[] = [];
    const devices: MediaDeviceInfo[] =
        await MediaDevicesProxy.getMediaDevices().enumerateDevices();

    devices.forEach((device) => {
      if (device.kind === 'videoinput') {
        connectedCameraNames.push(device.label);
      }
    });

    this.connectedCameraNames_ = connectedCameraNames;
  }

  private computeIsCameraListEmpty_(): boolean {
    return this.connectedCameraNames_.length === 0;
  }

  private computeOnOffText_(): string {
    return this.isCameraAllowed_() ? this.i18n('deviceOn') :
                                     this.i18n('deviceOff');
  }

  private computeCameraAccessStateText_(): string {
    if (this.isCameraAllowed_()) {
      return this.cameraFallbackMechanismEnabled_ ?
          this.i18n('privacyHubCameraSubpageCameraToggleFallbackSubtext') :
          this.i18n('privacyHubCameraSubpageCameraToggleSubtext');
    } else {
      return this.i18n('privacyHubCameraAccessBlockedText');
    }
  }

  private computeShouldDisableCameraToggle_(): boolean {
    return this.cameraSwitchForceDisabled_ || this.isCameraListEmpty_;
  }

  private getCameraToggle_(): CrToggleElement {
    return castExists(
        this.shadowRoot!.querySelector<CrToggleElement>('#cameraToggle'));
  }

  private onAccessStatusRowClick_(): void {
    if (this.shouldDisableCameraToggle_) {
      return;
    }

    this.getCameraToggle_().click();
  }

  private onManagePermissionsInChromeRowClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        CAMERA_SUBPAGE_USER_ACTION_HISTOGRAM_NAME,
        PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED,
        NUMBER_OF_POSSIBLE_USER_ACTIONS);

    this.mojoInterfaceProvider_.openBrowserPermissionSettings(
        PermissionType.kCamera);
  }

  private onCameraToggleClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        CAMERA_SUBPAGE_USER_ACTION_HISTOGRAM_NAME,
        PrivacyHubSensorSubpageUserAction.SYSTEM_ACCESS_CHANGED,
        NUMBER_OF_POSSIBLE_USER_ACTIONS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubCameraSubpage.is]: SettingsPrivacyHubCameraSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubCameraSubpage.is, SettingsPrivacyHubCameraSubpage);
