// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-page' contains privacy hub configurations.
 */

import '../../controls/settings_toggle_button.js';
import './metrics_consent_toggle_button.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';

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
    this.addWebUIListener('camera-hardware-toggle-changed', (enabled) => {
      this.setCameraHardwareToggleState_(enabled);
    });
    this.browserProxy_.getInitialCameraHardwareToggleState().then((enabled) => {
      this.setCameraHardwareToggleState_(enabled);
    });
    this.addWebUIListener('microphone-hardware-toggle-changed', (enabled) => {
      this.setMicrophoneHardwareToggleState_(enabled);
    });
    this.browserProxy_.getInitialMicrophoneHardwareToggleState().then(
        (enabled) => {
          this.setMicrophoneHardwareToggleState_(enabled);
        });
    this.addWebUIListener(
        'availability-of-microphone-for-simple-usage-changed', (available) => {
          this.setMicrophoneForSimpleUsageAvailable_(available);
        });
    this.browserProxy_.getInitialAvailabilityOfMicrophoneForSimpleUsage().then(
        (available) => {
          this.setMicrophoneForSimpleUsageAvailable_(available);
        });
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

      /** @private {string} */
      cameraToggleActive_: {
        type: String,
        value: '',
      },

      /** @private {boolean} */
      microphoneHardwareToggleActive_: {
        type: Boolean,
        value: false,
      },

      /** @private {string} */
      microphoneToggleSubLabel_: {
        type: String,
        computed: 'computeMicrophoneToggleSubLabel_(' +
            'microphoneHardwareToggleActive_, ' +
            'microphoneForSimpleUsageAvailable_)',
      },

      /** @private {boolean} */
      microphoneForSimpleUsageAvailable_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      shouldDisableMicrophoneToggle_: {
        type: Boolean,
        computed: 'computeShouldDisableMicrophoneToggle_(' +
            'microphoneHardwareToggleActive_, ' +
            'microphoneForSimpleUsageAvailable_)',
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
   * @param {boolean} enabled
   * @private
   */
  setCameraHardwareToggleState_(enabled) {
    if (enabled) {
      this.cameraToggleActive_ = this.i18n('cameraToggleSublabelActive');
    } else {
      this.cameraToggleActive_ = '';
    }
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
   * @return {string} The microphone toggle sublabel.
   * @private
   */
  computeMicrophoneToggleSubLabel_() {
    if (this.microphoneHardwareToggleActive_) {
      return this.i18n('microphoneToggleSublabelHWToggleActive');
    } else if (!this.microphoneForSimpleUsageAvailable_) {
      return this.i18n('microphoneToggleSublabelNoMicConnected');
    } else {
      return '';
    }
  }

  /**
   * @param {boolean} available
   * @private
   */
  setMicrophoneForSimpleUsageAvailable_(available) {
    this.microphoneForSimpleUsageAvailable_ = available;
  }

  /**
   * @return {boolean} Whether privacy hub microphone toggle should be disabled.
   * @private
   */
  computeShouldDisableMicrophoneToggle_() {
    return this.microphoneHardwareToggleActive_ ||
        !this.microphoneForSimpleUsageAvailable_;
  }
}

customElements.define(SettingsPrivacyHubPage.is, SettingsPrivacyHubPage);
