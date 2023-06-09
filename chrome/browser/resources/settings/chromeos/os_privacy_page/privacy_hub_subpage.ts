// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-subpage' contains privacy hub configurations.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './metrics_consent_toggle_button.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {MediaDevicesProxy} from './media_devices_proxy.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {getTemplate} from './privacy_hub_subpage.html.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * Keep in sync with PrivacyHubNavigationOrigin in
 * tools/metrics/histograms/enums.xml and
 * ash/system/privacy_hub/privacy_hub_metrics.h.
 */
export const PrivacyHubNavigationOrigin = {
  SYSTEM_SETTINGS: 0,
  NOTIFICATION: 1,
};

const SettingsPrivacyHubSubpageBase = PrefsMixin(DeepLinkingMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsPrivacyHubSubpage extends SettingsPrivacyHubSubpageBase {
  static get is() {
    return 'settings-privacy-hub-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the part of privacy hub for dogfooding should be displayed.
       */
      showPrivacyHubMVPPage_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPrivacyHubMVPPage');
        },
      },

      /**
       * The list of connected cameras.
       */
      camerasConnected_: {
        type: Array,
        value: [],
      },

      isCameraListEmpty_: {
        type: Boolean,
        computed: 'computeIsCameraListEmpty_(camerasConnected_)',
      },

      /**
       * The list of connected microphones.
       */
      microphonesConnected_: {
        type: Array,
        value: [],
      },

      isMicListEmpty_: {
        type: Boolean,
        computed: 'computeIsMicListEmpty_(microphonesConnected_)',
      },

      microphoneHardwareToggleActive_: {
        type: Boolean,
        value: false,
      },

      shouldDisableMicrophoneToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableMicrophoneToggle_(isMicListEmpty_, ' +
            'microphoneHardwareToggleActive_)',
      },

      /**
       * Whether the part of speak-on-mute detection should be displayed.
       */
      showSpeakOnMuteDetectionPage_: {
        type: Boolean,
        readOnly: true,
        value: () => {
          return loadTimeData.getBoolean('showSpeakOnMuteDetectionPage');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kCameraOnOff,
          Setting.kMicrophoneOnOff,
          Setting.kSpeakOnMuteDetectionOnOff,
          Setting.kGeolocationOnOff,
          Setting.kUsageStatsAndCrashReports,
        ]),
      },
    };
  }

  private browserProxy_: PrivacyHubBrowserProxy;
  private camerasConnected_: string[];
  private isCameraListEmpty_: boolean;
  private isMicListEmpty_: boolean;
  private microphonesConnected_: string[];
  private microphoneHardwareToggleActive_: boolean;
  private shouldDisableMicrophoneToggle_: boolean;
  private showPrivacyHubMVPPage_: boolean;
  private showSpeakOnMuteDetectionPage_: boolean;

  constructor() {
    super();

    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();
    assert(loadTimeData.getBoolean('showPrivacyHubPage'));

    this.addWebUiListener(
        'microphone-hardware-toggle-changed', (enabled: boolean) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });
    this.browserProxy_.getInitialMicrophoneHardwareToggleState().then(
        (enabled) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });

    this.updateMediaDeviceLists_();
    MediaDevicesProxy.getMediaDevices().addEventListener(
        'devicechange', () => this.updateMediaDeviceLists_());
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PRIVACY_HUB) {
      return;
    }
    this.attemptDeepLink();
  }

  /**
   * @return Whether the list of cameras displayed in this page is empty.
   */
  private computeIsCameraListEmpty_(): boolean {
    return this.camerasConnected_.length === 0;
  }

  /**
   * @return Whether the list of microphones displayed in this page is empty.
   */
  private computeIsMicListEmpty_(): boolean {
    return this.microphonesConnected_.length === 0;
  }

  private setMicrophoneHardwareToggleState_(enabled: boolean): void {
    if (enabled) {
      this.microphoneHardwareToggleActive_ = true;
    } else {
      this.microphoneHardwareToggleActive_ = false;
    }
  }

  /**
   * @return Whether privacy hub microphone toggle should be disabled.
   */
  private computeShouldDisableMicrophoneToggle_(): boolean {
    return this.microphoneHardwareToggleActive_ || this.isMicListEmpty_;
  }

  private updateMediaDeviceLists_(): void {
    MediaDevicesProxy.getMediaDevices().enumerateDevices().then((devices) => {
      const connectedCameras: string[] = [];
      const connectedMicrophones: string[] = [];
      devices.forEach((device) => {
        if (device.kind === 'videoinput') {
          connectedCameras.push(device.label);
        } else if (
            device.kind === 'audioinput' && device.deviceId !== 'default') {
          connectedMicrophones.push(device.label);
        }
      });
      this.camerasConnected_ = connectedCameras;
      this.microphonesConnected_ = connectedMicrophones;
    });
  }

  private onCameraToggleChanged_(event: Event): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.PrivacyHub.Camera.Settings.Enabled',
        (event.target as SettingsToggleButtonElement).checked);
  }

  private onMicrophoneToggleChanged_(event: Event): void {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.PrivacyHub.Microphone.Settings.Enabled',
        (event.target as SettingsToggleButtonElement).checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubSubpage.is]: SettingsPrivacyHubSubpage;
  }
}

customElements.define(SettingsPrivacyHubSubpage.is, SettingsPrivacyHubSubpage);
