// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-hub-microphone-subpage' contains a detailed overview about
 * the state of the system microphone access.
 */

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MediaDevicesProxy} from './media_devices_proxy.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {getTemplate} from './privacy_hub_microphone_subpage.html.js';

const SettingsPrivacyHubMicrophoneSubpageBase =
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsPrivacyHubMicrophoneSubpage extends
    SettingsPrivacyHubMicrophoneSubpageBase {
  static get is() {
    return 'settings-privacy-hub-microphone-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The list of microphones connected to the device.
       */
      connectedMicrophones_: {
        type: Array,
        value: [],
      },

      /**
       * Indicates whether `connectedMicrophones_` is empty.
       */
      isMicListEmpty_: {
        type: Boolean,
        computed: 'computeIsMicListEmpty_(connectedMicrophones_)',
      },

      /**
       * Indicates whether the microphone hardware toggle is active.
       */
      microphoneHardwareToggleActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the `cr-toggle` for microphone should be disabled.
       */
      shouldDisableMicrophoneToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableMicrophoneToggle_(isMicListEmpty_, ' +
            'microphoneHardwareToggleActive_)',
      },
    };
  }

  private browserProxy_: PrivacyHubBrowserProxy;
  private isMicListEmpty_: boolean;
  private microphoneHardwareToggleActive_: boolean;
  private connectedMicrophones_: string[];
  private shouldDisableMicrophoneToggle_: boolean;

  constructor() {
    super();

    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'microphone-hardware-toggle-changed', (enabled: boolean) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });
    this.browserProxy_.getInitialMicrophoneHardwareToggleState().then(
        (enabled) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });

    this.updateMicrophoneList_();
    MediaDevicesProxy.getMediaDevices().addEventListener(
        'devicechange', () => this.updateMicrophoneList_());
  }

  private setMicrophoneHardwareToggleState_(enabled: boolean): void {
    this.microphoneHardwareToggleActive_ = enabled;
  }

  private async updateMicrophoneList_(): Promise<void> {
    const connectedMicrophones: string[] = [];
    const devices: MediaDeviceInfo[] =
        await MediaDevicesProxy.getMediaDevices().enumerateDevices();

    devices.forEach((device) => {
      if (device.kind === 'audioinput' && device.deviceId !== 'default') {
        connectedMicrophones.push(device.label);
      }
    });

    this.connectedMicrophones_ = connectedMicrophones;
  }

  private computeIsMicListEmpty_(): boolean {
    return this.connectedMicrophones_.length === 0;
  }

  private computeOnOffText_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ? this.i18n('deviceOn') : this.i18n('deviceOff');
  }

  private computeOnOffSubtext_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ? this.i18n('microphoneToggleSubtext') :
                               'Blocked for all';
  }

  private computeShouldDisableMicrophoneToggle_(): boolean {
    return this.microphoneHardwareToggleActive_ || this.isMicListEmpty_;
  }

  private onManagePermissionsInChromeRowClick_(): void {
    window.open('chrome://settings/content/microphone');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubMicrophoneSubpage.is]:
        SettingsPrivacyHubMicrophoneSubpage;
  }
}

customElements.define(
    SettingsPrivacyHubMicrophoneSubpage.is,
    SettingsPrivacyHubMicrophoneSubpage);
