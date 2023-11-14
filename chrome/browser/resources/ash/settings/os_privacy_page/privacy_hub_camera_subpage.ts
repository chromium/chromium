// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-camera-subpage' contains a detailed overview about the
 * state of the system camera access.
 */

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {MediaDevicesProxy} from './media_devices_proxy.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {getTemplate} from './privacy_hub_camera_subpage.html.js';

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

  private browserProxy_: PrivacyHubBrowserProxy;
  private cameraSwitchForceDisabled_: boolean;
  private connectedCameras_: string[];
  private isCameraListEmpty_: boolean;
  private shouldDisableCameraToggle_: boolean;

  constructor() {
    super();

    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
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
    this.getCameraToggle_().click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubCameraSubpage.is]: SettingsPrivacyHubCameraSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubCameraSubpage.is, SettingsPrivacyHubCameraSubpage);
