// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-text-to-speech-page' is the accessibility settings subpage
 * for text-to-speech accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin, PrefsMixinInterface} from '../../prefs/prefs_mixin.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {routes} from '../os_route.js';
import {RouteOriginMixin, RouteOriginMixinInterface} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './text_to_speech_page.html.js';
import {TextToSpeechPageBrowserProxy, TextToSpeechPageBrowserProxyImpl} from './text_to_speech_page_browser_proxy.js';

const SettingsTextToSpeechPageElementBase =
    mixinBehaviors(
        [
          DeepLinkingBehavior,
        ],
        RouteOriginMixin(
            PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUiListenerMixinInterface & PrefsMixinInterface &
          RouteOriginMixinInterface & DeepLinkingBehaviorInterface,
    };

class SettingsTextToSpeechPageElement extends
    SettingsTextToSpeechPageElementBase {
  static get is() {
    return 'settings-text-to-speech-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * |hasKeyboard_| starts undefined so observer doesn't trigger until it
       * has been populated.
       */
      hasKeyboard_: Boolean,

      isAccessibilitySelectToSpeakPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilitySelectToSpeakPageMigrationEnabled');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
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

  private deviceBrowserProxy_: DevicePageBrowserProxy;
  private hasKeyboard_: boolean;
  private route_: Route;
  private textToSpeechBrowserProxy_: TextToSpeechPageBrowserProxy;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_TEXT_TO_SPEECH;

    this.textToSpeechBrowserProxy_ =
        TextToSpeechPageBrowserProxyImpl.getInstance();

    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'has-hardware-keyboard',
        (hasKeyboard: boolean) => this.set('hasKeyboard_', hasKeyboard));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  }

  override ready() {
    super.ready();

    this.addFocusConfig(
        routes.A11Y_SELECT_TO_SPEAK, '#select-to-speak-subpage-trigger');
    this.addFocusConfig(routes.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_TEXT_TO_SPEECH) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Return ChromeVox description text based on whether ChromeVox is enabled.
   */
  private getChromeVoxDescription_(enabled: boolean): string {
    return this.i18n(
        enabled ? 'chromeVoxDescriptionOn' : 'chromeVoxDescriptionOff');
  }

  /**
   * Return Select-to-Speak description text based on:
   *    1. Whether Select-to-Speak is enabled.
   *    2. If it is enabled, whether a physical keyboard is present.
   */
  private getSelectToSpeakDescription_(enabled: boolean, hasKeyboard: boolean):
      string {
    if (!enabled) {
      return this.i18n('selectToSpeakDisabledDescription');
    }
    if (hasKeyboard) {
      return this.i18n('selectToSpeakDescription');
    }
    return this.i18n('selectToSpeakDescriptionWithoutKeyboard');
  }

  private onManageTtsSettingsTap_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS);
  }

  private onChromeVoxSettingsTap_(): void {
    this.textToSpeechBrowserProxy_.showChromeVoxSettings();
  }

  private onChromeVoxTutorialTap_(): void {
    this.textToSpeechBrowserProxy_.showChromeVoxTutorial();
  }

  private onSelectToSpeakSettingsTap_(): void {
    this.textToSpeechBrowserProxy_.showSelectToSpeakSettings();
  }

  private onSelectToSpeakTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_SELECT_TO_SPEAK);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-text-to-speech-page': SettingsTextToSpeechPageElement;
  }
}

customElements.define(
    SettingsTextToSpeechPageElement.is, SettingsTextToSpeechPageElement);
