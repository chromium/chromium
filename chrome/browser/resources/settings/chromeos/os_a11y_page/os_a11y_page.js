// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-a11y-page' is the small section of advanced settings containing
 * a subpage with Accessibility settings for ChromeOS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../a11y_page/captions_subpage.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import './manage_a11y_page.js';
import './text_to_speech_page.js';
import './display_and_magnification_page.js';
import './keyboard_and_text_input_page.js';
import './cursor_and_touchpad_page.js';
import './audio_and_captions_page.js';
import './switch_access_subpage.js';
import './tts_subpage.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page_browser_proxy.js';

/**
 * TODO(dpapad): Remove this when os_a11y_page.js is migrated to TypeScript.
 * @interface
 */
class SettingsCaptionsElement {
  /** @return {SettingsToggleButtonElement} */
  getLiveCaptionToggle() {}
}

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const OsSettingsA11YPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class OsSettingsA11YPageElement extends OsSettingsA11YPageElementBase {
  static get is() {
    return 'os-settings-a11y-page';
  }

  static get template() {
    return html`{__html_template__}`;
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
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Whether to show accessibility labels settings.
       */
      showAccessibilityLabelsSetting_: {
        type: Boolean,
        value: false,
      },

      /** @protected {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.MANAGE_ACCESSIBILITY) {
            map.set(routes.MANAGE_ACCESSIBILITY.path, '#subpage-trigger');
          }
          if (routes.A11Y_TEXT_TO_SPEECH) {
            map.set(
                routes.A11Y_TEXT_TO_SPEECH.path,
                '#text-to-speech-page-trigger');
          }
          if (routes.A11Y_DISPLAY_AND_MAGNIFICATION) {
            map.set(
                routes.A11Y_DISPLAY_AND_MAGNIFICATION.path,
                '#display-and-magnification-page-trigger');
          }
          if (routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
            map.set(
                routes.A11Y_KEYBOARD_AND_TEXT_INPUT.path,
                '#keyboard-and-text-input-page-trigger');
          }
          if (routes.A11Y_CURSOR_AND_TOUCHPAD) {
            map.set(
                routes.A11Y_CURSOR_AND_TOUCHPAD.path,
                '#cursor-and-touchpad-page-trigger');
          }
          if (routes.A11Y_AUDIO_AND_CAPTIONS) {
            map.set(
                routes.A11Y_AUDIO_AND_CAPTIONS.path,
                '#audio-and-captions-page-trigger');
          }
          return map;
        },
      },

      /** @protected */
      isAccessibilityOSSettingsVisibilityEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityOSSettingsVisibilityEnabled');
        },
      },

      /**
       * Whether the user is in kiosk mode.
       * @protected
       */
      isKioskModeActive_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /**
       * Whether the user is in guest mode.
       * @protected
       */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kA11yQuickSettings,
          Setting.kGetImageDescriptionsFromGoogle,
          Setting.kLiveCaption,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!OsA11yPageBrowserProxy} */
    this.browserProxy_ = OsA11yPageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'screen-reader-state-changed',
        hasScreenReader => this.onScreenReaderStateChanged_(hasScreenReader));

    // Enables javascript and gets the screen reader state.
    this.browserProxy_.a11yPageReady();
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId === Setting.kLiveCaption) {
      afterNextRender(this, () => {
        const captionsSubpage = /** @type {?SettingsCaptionsElement} */ (
            this.shadowRoot.querySelector('settings-captions'));
        if (captionsSubpage && captionsSubpage.getLiveCaptionToggle()) {
          this.showDeepLinkElement(/** @type {!SettingsToggleButtonElement} */ (
              captionsSubpage.getLiveCaptionToggle()));
          return;
        }
        console.warn(`Element with deep link id ${settingId} not focusable.`);
      });

      // Stop deep link attempt since we completed it manually.
      return false;
    }

    // Continue with deep linking attempt.
    return true;
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    if (route === routes.OS_ACCESSIBILITY ||
        route === routes.MANAGE_CAPTION_SETTINGS) {
      this.attemptDeepLink();
    }
  }

  /**
   * Whether additional features link should be shown.
   * @param {boolean} isKiosk
   * @param {boolean} isGuest
   * @return {boolean}
   * @private
   */
  shouldShowAdditionalFeaturesLink_(isKiosk, isGuest) {
    return !isKiosk && !isGuest;
  }

  /**
   * @private
   * @param {boolean} hasScreenReader Whether a screen reader is enabled.
   */
  onScreenReaderStateChanged_(hasScreenReader) {
    this.showAccessibilityLabelsSetting_ = hasScreenReader;
  }

  /** @private */
  onToggleAccessibilityImageLabels_() {
    const a11yImageLabelsOn = this.$.a11yImageLabels.checked;
    if (a11yImageLabelsOn) {
      this.browserProxy_.confirmA11yImageLabels();
    }
  }

  /** @private */
  onManageAccessibilityFeaturesTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  }

  /** @private */
  onTextToSpeechTap_() {
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  }

  /** @private */
  onDisplayAndMagnificationTap_() {
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  }

  /** @private */
  onKeyboardAndTextInputTap_() {
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  }

  /** @private */
  onCursorAndTouchpadTap_() {
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD);
  }

  /** @private */
  onAudioAndCaptionsTap_() {
    Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS);
  }

  /** @private */
  onAdditionalFeaturesClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }
}

customElements.define(OsSettingsA11YPageElement.is, OsSettingsA11YPageElement);
