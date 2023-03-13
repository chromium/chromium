// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-a11y-page' is the small section of advanced settings containing
 * a subpage with Accessibility settings for ChromeOS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/settings_toggle_button.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import './manage_a11y_page.js';
import './text_to_speech_page.js';
import './display_and_magnification_page.js';
import './keyboard_and_text_input_page.js';
import './cursor_and_touchpad_page.js';
import './audio_and_captions_page.js';
import './chromevox_subpage.js';
import './select_to_speak_subpage.js';
import './switch_access_subpage.js';
import './tts_subpage.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_a11y_page.html.js';
import {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page_browser_proxy.js';

interface OsSettingsA11yPageElement {
  $: {
    a11yImageLabels: SettingsToggleButtonElement,
  };
}

const OsSettingsA11yPageElementBase = DeepLinkingMixin(
    RouteOriginMixin(PrefsMixin(WebUiListenerMixin(PolymerElement))));

class OsSettingsA11yPageElement extends OsSettingsA11yPageElementBase {
  static get is() {
    return 'os-settings-a11y-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current active route.
       */
      currentRoute: {
        type: Object,
        notify: true,
      },

      /**
       * Whether a screen reader is enabled.
       */
      hasScreenReader_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to show accessibility labels settings.
       */
      showAccessibilityLabelsSetting_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether ChromeVox page migration is enabled.
       */
      isAccessibilityChromeVoxPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityChromeVoxPageMigrationEnabled');
        },
      },

      /**
       * Whether Select-to-speak page migration is enabled.
       */
      isAccessibilitySelectToSpeakPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilitySelectToSpeakPageMigrationEnabled');
        },
      },

      /**
       * Whether the user is in kiosk mode.
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /**
       * Whether the user is in guest mode.
       */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kA11yQuickSettings,
          Setting.kGetImageDescriptionsFromGoogle,
          Setting.kLiveCaption,
        ]),
      },
    };
  }

  currentRoute: Route;
  private browserProxy_: OsA11yPageBrowserProxy;
  private hasScreenReader_: boolean;
  private isAccessibilityOSSettingsVisibilityEnabled_: boolean;
  private isGuest_: boolean;
  private isKioskModeActive_: boolean;
  private route_: Route;
  private showAccessibilityLabelsSetting_: boolean;
  private isAccessibilityChromeVoxPageMigrationEnabled_: boolean;
  private isAccessibilitySelectToSpeakPageMigrationEnabled_: boolean;

  constructor() {
    super();

    this.route_ = routes.OS_ACCESSIBILITY;
    this.browserProxy_ = OsA11yPageBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

    if (routes.MANAGE_ACCESSIBILITY) {
      this.addFocusConfig(routes.MANAGE_ACCESSIBILITY, '#subpage-trigger');
    }
    if (routes.A11Y_TEXT_TO_SPEECH) {
      this.addFocusConfig(
          routes.A11Y_TEXT_TO_SPEECH, '#text-to-speech-page-trigger');
    }
    if (routes.A11Y_DISPLAY_AND_MAGNIFICATION) {
      this.addFocusConfig(
          routes.A11Y_DISPLAY_AND_MAGNIFICATION,
          '#display-and-magnification-page-trigger');
    }
    if (routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
      this.addFocusConfig(
          routes.A11Y_KEYBOARD_AND_TEXT_INPUT,
          '#keyboard-and-text-input-page-trigger');
    }
    if (routes.A11Y_CURSOR_AND_TOUCHPAD) {
      this.addFocusConfig(
          routes.A11Y_CURSOR_AND_TOUCHPAD, '#cursor-and-touchpad-page-trigger');
    }
    if (routes.A11Y_AUDIO_AND_CAPTIONS) {
      this.addFocusConfig(
          routes.A11Y_AUDIO_AND_CAPTIONS, '#audio-and-captions-page-trigger');
    }

    this.addWebUiListener(
        'screen-reader-state-changed',
        (hasScreenReader: boolean) =>
            this.onScreenReaderStateChanged_(hasScreenReader));

    // Enables javascript and gets the screen reader state.
    this.browserProxy_.a11yPageReady();
  }

  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    if (newRoute === routes.OS_ACCESSIBILITY) {
      this.attemptDeepLink();
    }
  }

  private shouldShowAdditionalFeaturesLink_(isKiosk: boolean, isGuest: boolean):
      boolean {
    return !isKiosk && !isGuest;
  }

  private onScreenReaderStateChanged_(hasScreenReader: boolean): void {
    this.hasScreenReader_ = hasScreenReader;
    this.showAccessibilityLabelsSetting_ = this.hasScreenReader_;
  }

  private onToggleAccessibilityImageLabels_(): void {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      this.browserProxy_.confirmA11yImageLabels();
    }
  }

  private onManageAccessibilityFeaturesTap_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  }

  private onTextToSpeechTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  }

  private onDisplayAndMagnificationTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  }

  private onKeyboardAndTextInputTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  }

  private onCursorAndTouchpadTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD);
  }

  private onAudioAndCaptionsTap_(): void {
    Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS);
  }

  private onAdditionalFeaturesClick_(): void {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-a11y-page': OsSettingsA11yPageElement;
  }
}

customElements.define(OsSettingsA11yPageElement.is, OsSettingsA11yPageElement);
