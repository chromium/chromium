// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-text-to-speech-subpage' is the accessibility settings subpage
 * for text-to-speech accessibility settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './text_to_speech_subpage.html.js';
import {TextToSpeechSubpageBrowserProxy, TextToSpeechSubpageBrowserProxyImpl} from './text_to_speech_subpage_browser_proxy.js';

/**
 * Numerical values should not be changed because they must stay in sync with
 * screen_ai::ScreenAIInstallState::State defined in screen_ai_install_state.h
 */
export enum ScreenAiInstallStatus {
  NOT_DOWNLOADED = 0,
  DOWNLOADING = 1,
  DOWNLOAD_FAILED = 2,
  DOWNLOADED = 3,
}

const SettingsTextToSpeechSubpageElementBase =
    DeepLinkingMixin(RouteOriginMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsTextToSpeechSubpageElement extends
    SettingsTextToSpeechSubpageElementBase {
  static get is() {
    return 'settings-text-to-speech-subpage' as const;
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

      /**
       * |hasScreenReader| is being passed from os_a11y_page.html on page load.
       * Indicate whether a screen reader is enabled.
       */
      hasScreenReader: Boolean,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kChromeVox,
          Setting.kSelectToSpeak,
        ]),
      },
    };
  }

  hasScreenReader: boolean;
  private deviceBrowserProxy_: DevicePageBrowserProxy;
  private hasKeyboard_: boolean;
  private textToSpeechBrowserProxy_: TextToSpeechSubpageBrowserProxy;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.A11Y_TEXT_TO_SPEECH;

    this.textToSpeechBrowserProxy_ =
        TextToSpeechSubpageBrowserProxyImpl.getInstance();

    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'has-hardware-keyboard',
        (hasKeyboard: boolean) => this.set('hasKeyboard_', hasKeyboard));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.A11Y_SELECT_TO_SPEAK, '#select-to-speak-subpage-trigger');
    this.addFocusConfig(routes.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
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

  private onManageTtsSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS);
  }

  private onChromeVoxSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_CHROMEVOX);
  }

  private onChromeVoxTutorialClick_(): void {
    this.textToSpeechBrowserProxy_.showChromeVoxTutorial();
  }

  private onSelectToSpeakSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_SELECT_TO_SPEAK);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsTextToSpeechSubpageElement.is]: SettingsTextToSpeechSubpageElement;
  }
}

customElements.define(
    SettingsTextToSpeechSubpageElement.is, SettingsTextToSpeechSubpageElement);
