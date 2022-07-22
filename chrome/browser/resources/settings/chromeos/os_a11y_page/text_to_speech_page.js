// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-text-to-speech-page' is the accessibility settings subpage
 * for text-to-speech accessibility settings.
 */

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {TextToSpeechPageBrowserProxy, TextToSpeechPageBrowserProxyImpl} from './text_to_speech_page_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsTextToSpeechPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsTextToSpeechPageElement extends
    SettingsTextToSpeechPageElementBase {
  static get is() {
    return 'settings-text-to-speech-page';
  }

  static get template() {
    return html`{__html_template__}`;
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
       * |hasKeyboard_| starts undefined so observer doesn't trigger until it
       * has been populated.
       * @private
       */
      hasKeyboard_: Boolean,

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kChromeVox,
          Setting.kSelectToSpeak,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_TEXT_TO_SPEECH;

    /** @private {!TextToSpeechPageBrowserProxy} */
    this.textToSpeechBrowserProxy_ =
        TextToSpeechPageBrowserProxyImpl.getInstance();

    /** @private {!DevicePageBrowserProxy} */
    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'has-hardware-keyboard',
        (hasKeyboard) => this.set('hasKeyboard_', hasKeyboard));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  }

  /** @override */
  ready() {
    super.ready();

    this.addFocusConfig(routes.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginBehavior implementation
   * @param {!Route} newRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {
    RouteOriginBehaviorImpl.currentRouteChanged.call(this, newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_TEXT_TO_SPEECH) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Return ChromeVox description text based on whether ChromeVox is enabled.
   * @param {boolean} enabled
   * @return {string}
   * @private
   */
  getChromeVoxDescription_(enabled) {
    return this.i18n(
        enabled ? 'chromeVoxDescriptionOn' : 'chromeVoxDescriptionOff');
  }

  /**
   * Return Select-to-Speak description text based on:
   *    1. Whether Select-to-Speak is enabled.
   *    2. If it is enabled, whether a physical keyboard is present.
   * @param {boolean} enabled
   * @param {boolean} hasKeyboard
   * @return {string}
   * @private
   */
  getSelectToSpeakDescription_(enabled, hasKeyboard) {
    if (!enabled) {
      return this.i18n('selectToSpeakDisabledDescription');
    }
    if (hasKeyboard) {
      return this.i18n('selectToSpeakDescription');
    }
    return this.i18n('selectToSpeakDescriptionWithoutKeyboard');
  }

  /** @private */
  onManageTtsSettingsTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS);
  }

  /** @private */
  onChromeVoxSettingsTap_() {
    this.textToSpeechBrowserProxy_.showChromeVoxSettings();
  }

  /** @private */
  onChromeVoxTutorialTap_() {
    this.textToSpeechBrowserProxy_.showChromeVoxTutorial();
  }

  /** @private */
  onSelectToSpeakSettingsTap_() {
    this.textToSpeechBrowserProxy_.showSelectToSpeakSettings();
  }
}

customElements.define(
    SettingsTextToSpeechPageElement.is, SettingsTextToSpeechPageElement);
