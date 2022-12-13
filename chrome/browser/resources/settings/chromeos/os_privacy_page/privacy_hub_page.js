// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-page' contains privacy hub configurations.
 */

import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../../controls/settings_toggle_button.js';
import './metrics_consent_toggle_button.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {MediaDevicesProxy} from './media_devices_proxy.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';

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

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsPrivacyHubPageBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsPrivacyHubPage extends SettingsPrivacyHubPageBase {
  static get is() {
    return 'settings-privacy-hub-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!PrivacyHubBrowserProxy}  */
    this.browserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    assert(loadTimeData.getBoolean('showPrivacyHubPage'));

    this.addWebUIListener('microphone-hardware-toggle-changed', (enabled) => {
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

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kCameraOnOff,
          Setting.kMicrophoneOnOff,
          Setting.kGeolocationOnOff,
          Setting.kUsageStatsAndCrashReports,
        ]),
      },

      /**
       * Whether the part of privacy hub for dogfooding should be displayed.
       * @private
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
       * @private {Array<string>}
       */
      camerasConnected_: {
        type: Array,
        value: [],
      },

      /** @private {boolean} */
      isCameraListEmpty_: {
        type: Boolean,
        computed: 'computeIsCameraListEmpty_(camerasConnected_)',
      },

      /**
       * The list of connected microphones.
       * @private {Array<string>}
       */
      microphonesConnected_: {
        type: Array,
        value: [],
      },

      /** @private {boolean} */
      isMicListEmpty_: {
        type: Boolean,
        computed: 'computeIsMicListEmpty_(microphonesConnected_)',
      },

      /** @private {boolean} */
      microphoneHardwareToggleActive_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      shouldDisableMicrophoneToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableMicrophoneToggle_(isMicListEmpty_,  ' +
            'microphoneHardwareToggleActive_)',
      },
    };
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== routes.PRIVACY_HUB) {
      return;
    }
    this.attemptDeepLink();
  }

  /**
   * @return {boolean} Whether the list of cameras displayed in this page is
   *     empty.
   * @private
   */
  computeIsCameraListEmpty_() {
    return this.camerasConnected_.length === 0;
  }

  /**
   * @return {boolean} Whether the list of microphones displayed in this page is
   *     empty.
   * @private
   */
  computeIsMicListEmpty_() {
    return this.microphonesConnected_.length === 0;
  }

  /**
   * @param {boolean} enabled
   * @private
   */
  setMicrophoneHardwareToggleState_(enabled) {
    if (enabled) {
      this.microphoneHardwareToggleActive_ = true;
    } else {
      this.microphoneHardwareToggleActive_ = false;
    }
  }

  /**
   * @return {boolean} Whether privacy hub microphone toggle should be disabled.
   * @private
   */
  computeShouldDisableMicrophoneToggle_() {
    return this.microphoneHardwareToggleActive_ || this.isMicListEmpty_;
  }

  /** @private */
  updateMediaDeviceLists_() {
    MediaDevicesProxy.getMediaDevices().enumerateDevices().then((devices) => {
      const connectedCameras = [];
      const connectedMicrophones = [];
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

  /**
   * @param {!CustomEvent} event
   * @private
   */
  onCameraToggleChanged_(event) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.PrivacyHub.Camera.Settings.Enabled', event.target.checked);
  }

  /**
   * @param {!CustomEvent} event
   * @private
   */
  onMicrophoneToggleChanged_(event) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.PrivacyHub.Microphone.Settings.Enabled',
        event.target.checked);
  }
}

customElements.define(SettingsPrivacyHubPage.is, SettingsPrivacyHubPage);
