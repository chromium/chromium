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

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './text_to_speech_page.html.js';
import {TextToSpeechPageBrowserProxy, TextToSpeechPageBrowserProxyImpl} from './text_to_speech_page_browser_proxy.js';

/**
 * Numerical values should not be changed because they must stay in sync with
 * screen_ai::ScreenAIInstallState::State defined in screen_ai_install_state.h
 */
export enum ScreenAiInstallStatus {
  NOT_DOWNLOADED = 0,
  DOWNLOADING = 1,
  FAILED = 2,
  DOWNLOADED = 3,
  READY = 4,
}

const SettingsTextToSpeechPageElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsTextToSpeechPageElement extends
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

      /**
       * |hasScreenReader| is being passed from os_a11y_page.html on page load.
       * Indicate whether a screen reader is enabled.
       */
      hasScreenReader: Boolean,

      isAccessibilityChromeVoxPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityChromeVoxPageMigrationEnabled');
        },
      },

      isAccessibilitySelectToSpeakPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilitySelectToSpeakPageMigrationEnabled');
        },
      },

      /**
       * |pdfOcrProgress_| stores the downloading progress in percentage of
       * the ScreenAI library.
       */
      pdfOcrProgress_: Number,

      /**
       * |pdfOcrStatus_| stores the ScreenAI library install state.
       */
      pdfOcrStatus_: ScreenAiInstallStatus,

      /**
       * Whether to show the toggle button for PDF OCR.
       */
      showPdfOcrToggle_: {
        type: Boolean,
        computed: 'computeShowPdfOcrToggle_(hasScreenReader)',
      },

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
  private isAccessibilityChromeVoxPageMigrationEnabled_: boolean;
  private isAccessibilitySelectToSpeakPageMigrationEnabled_: boolean;
  private route_: Route;
  private pdfOcrProgress_: number;
  private pdfOcrStatus_: ScreenAiInstallStatus;
  private showPdfOcrToggle_: boolean;
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

    if (loadTimeData.getBoolean('pdfOcrEnabled')) {
      this.addWebUiListener(
          'pdf-ocr-state-changed',
          (pdfOcrState: ScreenAiInstallStatus) =>
              this.onPdfOcrStateChanged_(pdfOcrState));
      this.addWebUiListener(
          'pdf-ocr-downloading-progress-changed',
          (progress: number) =>
              this.onPdfOcrDownloadingProgressChanged_(progress));
      this.textToSpeechBrowserProxy_.pdfOcrSectionReady();
    }
  }

  override ready() {
    super.ready();

    this.addFocusConfig(
        routes.A11Y_SELECT_TO_SPEAK, '#select-to-speak-subpage-trigger');
    this.addFocusConfig(routes.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
  }

  private getPdfOcrToggleSublabel_(): string {
    switch (this.pdfOcrStatus_) {
      case ScreenAiInstallStatus.DOWNLOADING:
        return this.pdfOcrProgress_ > 0 && this.pdfOcrProgress_ < 100 ?
            this.i18n('pdfOcrDownloadProgressLabel', this.pdfOcrProgress_) :
            this.i18n('pdfOcrDownloadingLabel');
      case ScreenAiInstallStatus.FAILED:
        return this.i18n('pdfOcrDownloadErrorLabel');
      case ScreenAiInstallStatus.DOWNLOADED:
        return this.i18n('pdfOcrDownloadCompleteLabel');
      case ScreenAiInstallStatus.READY:
        // No subtitle update in this case
      case ScreenAiInstallStatus.NOT_DOWNLOADED:
        // No subtitle update in this case
      default:
        // This is a generic subtitle describing the feature.
        return this.i18n('pdfOcrSubtitle');
    }
  }

  private onPdfOcrStateChanged_(pdfOcrState: ScreenAiInstallStatus) {
    this.pdfOcrStatus_ = pdfOcrState;
  }

  private onPdfOcrDownloadingProgressChanged_(progress: number) {
    this.pdfOcrProgress_ = progress;
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
   * Return whether to show a PDF OCR toggle button based on:
   *    1. A PDF OCR feature flag is enabled.
   *    2. Whether a screen reader (i.e. ChromeVox) is enabled.
   */
  private computeShowPdfOcrToggle_(): boolean {
    return loadTimeData.getBoolean('pdfOcrEnabled') && this.hasScreenReader;
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
    this.textToSpeechBrowserProxy_.showChromeVoxSettings();
  }

  private onChromeVoxNewSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_CHROMEVOX);
  }

  private onChromeVoxTutorialClick_(): void {
    this.textToSpeechBrowserProxy_.showChromeVoxTutorial();
  }

  private onSelectToSpeakSettingsClick_(): void {
    this.textToSpeechBrowserProxy_.showSelectToSpeakSettings();
  }

  private onSelectToSpeakClick_(): void {
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
