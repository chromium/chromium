// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-subpage' contains privacy hub configurations.
 */

import '../app_management_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './metrics_consent_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {MediaDevicesProxy} from '../common/media_devices_proxy.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {Router, routes} from '../router.js';

import type {PrivacyHubBrowserProxy} from './privacy_hub_browser_proxy.js';
import {PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {GeolocationAccessLevel} from './privacy_hub_geolocation_subpage.js';
import {PrivacyHubSensorSubpageUserAction} from './privacy_hub_metrics_util.js';
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
       * Whether the location access control should be displayed in Privacy Hub.
       */
      showPrivacyHubLocationControl_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPrivacyHubLocationControl');
        },
      },

      locationSubLabel_: {
        type: String,
        computed: 'computeLocationRowSubtext_(' +
            'prefs.ash.user.geolocation_access_level.value)',
      },

      isCameraListEmpty_: {
        type: Boolean,
        value: true,
      },

      isMicListEmpty_: {
        type: Boolean,
        value: true,
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

      cameraFallbackMechanismEnabled_: {
        type: Boolean,
        value: false,
      },

      cameraRowSubtext_: {
        type: String,
        computed: 'computeCameraRowSubtext_(cameraFallbackMechanismEnabled_, ' +
            'prefs.ash.user.camera_allowed.*)',
      },

      microphoneRowSubtext_: {
        type: String,
        computed: 'computeMicrophoneRowSubtext_(' +
            'prefs.ash.user.microphone_allowed.*)',
      },

      microphoneToggleTooltipText_: {
        type: String,
        computed: 'computeMicrophoneToggleTooltipText_(isMicListEmpty_, ' +
            'microphoneHardwareToggleActive_)',
      },
    };
  }

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kCameraOnOff,
    Setting.kMicrophoneOnOff,
    Setting.kSpeakOnMuteDetectionOnOff,
    Setting.kGeolocationOnOff,
    Setting.kUsageStatsAndCrashReports,
  ]);

  private browserProxy_: PrivacyHubBrowserProxy;
  private showPrivacyHubLocationControl_: boolean;
  private locationSubLabel_: string;
  private cameraFallbackMechanismEnabled_: boolean;
  private cameraRowSubtext_: string;
  private isCameraListEmpty_: boolean;
  private isMicListEmpty_: boolean;
  private microphoneRowSubtext_: string;
  private microphoneHardwareToggleActive_: boolean;
  private microphoneToggleTooltipText_: string;
  private shouldDisableMicrophoneToggle_: boolean;
  private cameraSwitchForceDisabled_: boolean;
  private shouldDisableCameraToggle_: boolean;
  private showSpeakOnMuteDetectionPage_: boolean;

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

    this.updateMediaDeviceAvailability_();
    MediaDevicesProxy.getMediaDevices().addEventListener(
        'devicechange', () => this.updateMediaDeviceAvailability_());
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.PRIVACY_HUB) {
      return;
    }
    this.attemptDeepLink();
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

  /**
   * @return Whether privacy hub camera toggle should be disabled.
   */
  private computeShouldDisableCameraToggle_(): boolean {
    return this.cameraSwitchForceDisabled_ || this.isCameraListEmpty_;
  }

  private updateMediaDeviceAvailability_(): void {
    MediaDevicesProxy.getMediaDevices().enumerateDevices().then((devices) => {
      let cameraAvailable = false;
      let microphoneAvailable = false;
      devices.forEach((device) => {
        if (device.kind === 'videoinput') {
          cameraAvailable = true;
        } else if (
            device.kind === 'audioinput' && device.deviceId !== 'default') {
          microphoneAvailable = true;
        }
      });
      this.isCameraListEmpty_ = !cameraAvailable;
      this.isMicListEmpty_ = !microphoneAvailable;
    });
  }

  private onCameraSubpageLinkClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.CameraSubpage.UserAction',
        PrivacyHubSensorSubpageUserAction.SUBPAGE_OPENED,
        Object.keys(PrivacyHubSensorSubpageUserAction).length);

    Router.getInstance().navigateTo(routes.PRIVACY_HUB_CAMERA);
  }

  private onMicrophoneSubpageLinkClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
        PrivacyHubSensorSubpageUserAction.SUBPAGE_OPENED,
        Object.keys(PrivacyHubSensorSubpageUserAction).length);

    Router.getInstance().navigateTo(routes.PRIVACY_HUB_MICROPHONE);
  }

  private onGeolocationAreaClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.LocationSubpage.UserAction',
        PrivacyHubSensorSubpageUserAction.SUBPAGE_OPENED,
        Object.keys(PrivacyHubSensorSubpageUserAction).length);

    Router.getInstance().navigateTo(routes.PRIVACY_HUB_GEOLOCATION);
  }

  private computeLocationRowSubtext_(): string {
    if (!this.prefs) {
      return '';
    }

    const locationAccessLevel: GeolocationAccessLevel =
        this.getPref<GeolocationAccessLevel>(
                'ash.user.geolocation_access_level')
            .value;

    switch (locationAccessLevel) {
      case GeolocationAccessLevel.ALLOWED:
        return this.i18n('geolocationAreaAllowedSubtext');
      case GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM:
        return this.i18n('geolocationAreaOnlyAllowedForSystemSubtext');
      case GeolocationAccessLevel.DISALLOWED:
        return this.i18n('geolocationAreaDisallowedSubtext');
    }
  }

  private computeCameraRowSubtext_(): string {
    // Note: `this.getPref()` will assert the queried pref exists, but the prefs
    // property may not be initialized yet when this element runs the first
    // computation of this method. Ensure prefs is initialized first.
    if (!this.prefs) {
      return '';
    }

    const cameraAllowed = this.getPref<string>('ash.user.camera_allowed').value;
    if (cameraAllowed) {
      return this.cameraFallbackMechanismEnabled_ ?
          this.i18n('privacyHubPageCameraRowFallbackSubtext') :
          this.i18n('privacyHubPageCameraRowSubtext');
    }
    return this.i18n('privacyHubCameraAccessBlockedText');
  }

  private computeMicrophoneRowSubtext_(): string {
    const microphoneAllowed =
        this.getPref<string>('ash.user.microphone_allowed').value;
    return microphoneAllowed ?
        this.i18n('privacyHubPageMicrophoneRowSubtext') :
        this.i18n('privacyHubMicrophoneAccessBlockedText');
  }

  private computeMicrophoneToggleTooltipText_(): string {
    if (this.isMicListEmpty_) {
      return this.i18n('privacyHubNoMicrophoneConnectedTooltipText');
    } else if (this.microphoneHardwareToggleActive_) {
      return this.i18n('microphoneHwToggleTooltip');
    } else {
      return '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPrivacyHubSubpage.is]: SettingsPrivacyHubSubpage;
  }
}

customElements.define(SettingsPrivacyHubSubpage.is, SettingsPrivacyHubSubpage);
