// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-audio-and-captions-page' is the accessibility settings subpage for
 * audio and captions accessibility settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './captions_subpage.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './audio_and_captions_page.html.js';
import {AudioAndCaptionsPageBrowserProxy, AudioAndCaptionsPageBrowserProxyImpl} from './audio_and_captions_page_browser_proxy.js';

export enum NotificationColor {
  RED = 0xff0000,
  YELLOW = 0xffff00,
  GREEN = 0x00ff01,
  CYAN = 0x00ffff,
  BLUE = 0x0000fe,
  MAGENTA = 0xff00fe,
  PINK = 0xff017e,
}

export interface SettingsAudioAndCaptionsPageElement {
  $: {
    startupSoundEnabled: CrToggleElement,
  };
}

const SettingsAudioAndCaptionsPageElementBase =
    DeepLinkingMixin(RouteOriginMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsAudioAndCaptionsPageElement extends
    SettingsAudioAndCaptionsPageElementBase {
  static get is() {
    return 'settings-audio-and-captions-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
      },

      languageHelper: Object,

      /**
       * Whether the user is in kiosk mode.
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      isAccessibilityFlashNotificationFeatureEnabled: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityFlashNotificationFeatureEnabled');
        },
      },

      notificationColorOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: NotificationColor.RED,
              name: loadTimeData.getString('cursorColorRed'),
            },
            {
              value: NotificationColor.YELLOW,
              name: loadTimeData.getString('cursorColorYellow'),
            },
            {
              value: NotificationColor.GREEN,
              name: loadTimeData.getString('cursorColorGreen'),
            },
            {
              value: NotificationColor.CYAN,
              name: loadTimeData.getString('cursorColorCyan'),
            },
            {
              value: NotificationColor.BLUE,
              name: loadTimeData.getString('cursorColorBlue'),
            },
            {
              value: NotificationColor.MAGENTA,
              name: loadTimeData.getString('cursorColorMagenta'),
            },
            {
              value: NotificationColor.PINK,
              name: loadTimeData.getString('cursorColorPink'),
            },
          ];
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kFlashNotifications,
          Setting.kLiveCaption,
          Setting.kMonoAudio,
          Setting.kStartupSound,
        ]),
      },
    };
  }

  languages: LanguagesModel;
  languageHelper: LanguageHelper;
  private audioAndCaptionsBrowserProxy_: AudioAndCaptionsPageBrowserProxy;
  private isKioskModeActive_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.A11Y_AUDIO_AND_CAPTIONS;

    this.audioAndCaptionsBrowserProxy_ =
        AudioAndCaptionsPageBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'initial-data-ready',
        (startupSoundEnabled: boolean) =>
            this.updateStartupSoundEnabled_(startupSoundEnabled));
    this.addWebUiListener(
        'startup-sound-setting-retrieved',
        (startupSoundEnabled: boolean) =>
            this.updateStartupSoundEnabled_(startupSoundEnabled));
    this.audioAndCaptionsBrowserProxy_.audioAndCaptionsPageReady();
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId === Setting.kLiveCaption) {
      afterNextRender(this, () => {
        const captionsSubpage =
            this.shadowRoot!.querySelector('settings-captions');
        const toggle = captionsSubpage?.getLiveCaptionToggle();
        if (toggle) {
          this.showDeepLinkElement(toggle);
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
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.audioAndCaptionsBrowserProxy_.getStartupSoundEnabled();
    this.attemptDeepLink();
  }

  private toggleStartupSoundEnabled_(e: CustomEvent<boolean>): void {
    this.audioAndCaptionsBrowserProxy_.setStartupSoundEnabled(e.detail);
  }

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   */
  private updateStartupSoundEnabled_(startupSoundEnabled: boolean): void {
    this.$.startupSoundEnabled.checked = startupSoundEnabled;
  }

  private previewFlashNotification_(): void {
    this.audioAndCaptionsBrowserProxy_.previewFlashNotification();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-audio-and-captions-page': SettingsAudioAndCaptionsPageElement;
  }
}

customElements.define(
    SettingsAudioAndCaptionsPageElement.is,
    SettingsAudioAndCaptionsPageElement);
