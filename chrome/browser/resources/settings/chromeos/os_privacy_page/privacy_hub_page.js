// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-hub-page' contains privacy hub configurations.
 */

import '../../controls/settings_toggle_button.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
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
    assert(loadTimeData.getBoolean('showPrivacyHub'));
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
      microphoneHardwareToggleActiveSubLabel_: {
        type: String,
        computed: 'computeMicrophoneHardwareToggleSubLabel_(' +
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
   * @private
   */
  computeMicrophoneHardwareToggleSubLabel_() {
    return this.microphoneHardwareToggleActive_ ?
        this.i18n('microphoneToggleSublabelActive') :
        '';
  }
}

customElements.define(SettingsPrivacyHubPage.is, SettingsPrivacyHubPage);
