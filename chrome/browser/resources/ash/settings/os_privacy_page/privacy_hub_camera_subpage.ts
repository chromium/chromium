// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-camera-subpage' contains a detailed overview about the
 * state of the system camera access.
 */

import './privacy_hub_app_permission_row.js';

import {PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {isPermissionEnabled} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {App, AppPermissionsHandlerInterface, AppPermissionsObserverReceiver} from '../mojom-webui/app_permission_handler.mojom-webui.js';

import {MediaDevicesProxy} from './media_devices_proxy.js';
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
       */
      appList_: {
        type: Array,
        value: [],
      },

      connectedCameras_: {
        type: Array,
        value: [],
      },

      isCameraListEmpty_: {
        type: Boolean,
        computed: 'computeIsCameraListEmpty_(connectedCameras_)',
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

    };
  }

  private appList_: App[];
  private appPermissionsObserverReceiver_: AppPermissionsObserverReceiver|null;
  private browserProxy_: PrivacyHubBrowserProxy;
  private cameraSwitchForceDisabled_: boolean;
  private connectedCameras_: string[];
  private isCameraListEmpty_: boolean;
  private mojoInterfaceProvider_: AppPermissionsHandlerInterface;
  private shouldDisableCameraToggle_: boolean;

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

    this.updateAppList_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.appPermissionsObserverReceiver_!.$.close();
  }

  private async updateAppList_(): Promise<void> {
    const apps = (await this.mojoInterfaceProvider_.getApps()).apps;
    this.appList_ = apps.filter(hasCameraPermission);
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
    const connectedCameras: string[] = [];
    const devices: MediaDeviceInfo[] =
        await MediaDevicesProxy.getMediaDevices().enumerateDevices();

    devices.forEach((device) => {
      if (device.kind === 'videoinput') {
        connectedCameras.push(device.label);
      }
    });

    this.connectedCameras_ = connectedCameras;
  }

  private computeIsCameraListEmpty_(): boolean {
    return this.connectedCameras_.length === 0;
  }

  private computeOnOffText_(): string {
    const cameraAllowed = this.getPref<string>('ash.user.camera_allowed').value;
    return cameraAllowed ? this.i18n('deviceOn') : this.i18n('deviceOff');
  }

  private computeOnOffSubtext_(): string {
    const cameraAllowed = this.getPref<string>('ash.user.camera_allowed').value;
    return cameraAllowed ? this.i18n('cameraToggleSubtext') :
                           this.i18n('blockedForAllText');
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

    window.open('chrome://settings/content/camera');
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
