// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-a11y-page' is the small section of advanced settings containing
 * a subpage with Accessibility settings for ChromeOS.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_a11y_page.html.js';
import {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page_browser_proxy.js';

export interface OsSettingsA11yPageElement {
  $: {
    a11yImageLabelsToggle: SettingsToggleButtonElement,
  };
}

const OsSettingsA11yPageElementBase = DeepLinkingMixin(
    RouteOriginMixin(PrefsMixin(WebUiListenerMixin(PolymerElement))));

export class OsSettingsA11yPageElement extends OsSettingsA11yPageElementBase {
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

      section_: {
        type: Number,
        value: Section.kAccessibility,
        readOnly: true,
      },

      /**
       * Whether a screen reader is enabled.
       */
      hasScreenReader_: {
        type: Boolean,
        value: false,
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
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
      },

      languageHelper: Object,

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

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              imageDescription: 'os-settings:a11y-image-description',
              showInQuickSettings: 'os-settings:accessibility-revamp',
              textToSpeech: 'os-settings:text-to-speech',
              displayAndMagnification: 'os-settings:zoom-in',
              keyboardAndTextInput: 'os-settings:a11y-keyboard-and-text-input',
              cursorAndTouchpad: 'os-settings:cursor-click',
              audioAndCaptions: 'os-settings:a11y-hearing',
              findMore: 'os-settings:a11y-find-more',
            };
          }

          return {
            imageDescription: '',
            showInQuickSettings: '',
            textToSpeech: '',
            displayAndMagnification: '',
            keyboardAndTextInput: '',
            cursorAndTouchpad: '',
            audioAndCaptions: '',
            findMore: '',
          };
        },
      },
    };
  }

  currentRoute: Route;
  languages: LanguagesModel;
  languageHelper: LanguageHelper;

  private browserProxy_: OsA11yPageBrowserProxy;
  private hasScreenReader_: boolean;
  private isGuest_: boolean;
  private isKioskModeActive_: boolean;
  private rowIcons_: Record<string, string>;
  private section_: Section;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.OS_ACCESSIBILITY;

    this.browserProxy_ = OsA11yPageBrowserProxyImpl.getInstance();

    if (this.isKioskModeActive_) {
      this.redirectToOldA11ySettings();
    }
  }

  override ready(): void {
    super.ready();

    if (routes.A11Y_TEXT_TO_SPEECH) {
      this.addFocusConfig(
          routes.A11Y_TEXT_TO_SPEECH, '#textToSpeechSubpageTrigger');
    }
    if (routes.A11Y_DISPLAY_AND_MAGNIFICATION) {
      this.addFocusConfig(
          routes.A11Y_DISPLAY_AND_MAGNIFICATION,
          '#displayAndMagnificationPageTrigger');
    }
    if (routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
      this.addFocusConfig(
          routes.A11Y_KEYBOARD_AND_TEXT_INPUT,
          '#keyboardAndTextInputPageTrigger');
    }
    if (routes.A11Y_CURSOR_AND_TOUCHPAD) {
      this.addFocusConfig(
          routes.A11Y_CURSOR_AND_TOUCHPAD, '#cursorAndTouchpadPageTrigger');
    }
    if (routes.A11Y_AUDIO_AND_CAPTIONS) {
      this.addFocusConfig(
          routes.A11Y_AUDIO_AND_CAPTIONS, '#audioAndCaptionsPageTrigger');
    }
  }

  override connectedCallback(): void {
    super.connectedCallback();

    const updateScreenReaderState = (hasScreenReader: boolean): void => {
      this.hasScreenReader_ = hasScreenReader;
    };
    this.browserProxy_.getScreenReaderState().then(updateScreenReaderState);
    this.addWebUiListener(
        'screen-reader-state-changed', updateScreenReaderState);
  }

  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    if (newRoute === this.route) {
      this.attemptDeepLink();
    }
  }

  private redirectToOldA11ySettings(): void {
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  }

  private onToggleAccessibilityImageLabels_(): void {
    const a11yImageLabelsOn = this.$.a11yImageLabelsToggle.checked;
    if (a11yImageLabelsOn) {
      this.browserProxy_.confirmA11yImageLabels();
    }
  }

  private onSwitchAccessSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  private onTextToSpeechClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_TEXT_TO_SPEECH);
  }

  private onDisplayAndMagnificationClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  }

  private onKeyboardAndTextInputClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  }

  private onCursorAndTouchpadClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD);
  }

  private onAudioAndCaptionsClick_(): void {
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
