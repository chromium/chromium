// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-chromevox-subpage' is the accessibility settings subpage for
 * ChromeVox settings.
 */

import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../../controls/settings_dropdown_menu.js';
import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './chromevox_subpage.html.js';
import {ChromeVoxSubpageBrowserProxy, ChromeVoxSubpageBrowserProxyImpl} from './chromevox_subpage_browser_proxy.js';

const SYSTEM_VOICE = 'chromeos_system_voice';
const GOOGLE_TTS_EXTENSION_ID = 'gjjabgpgjpampikjhjpfhneeoapjbjaf';
const ESPEAK_TTS_EXTENSION_ID = 'dakbfdmgjiabojdgbiljlhgjbokobjpg';

/**
 * Represents a voice as sent from the TTS Handler class.
 * |name| is the user-facing voice name.
 * |remote| is whether the TTS voice is online (versus on-device).
 * |extensionId| is the Chrome Extension ID for the TTS voice.
 */
interface TtsHandlerVoice {
  name: string;
  remote: boolean;
  extensionId: string;
}

interface SettingsChromeVoxSubpageElement {
  $: {
    capitalStrategyDropdown: SettingsDropdownMenuElement,
  };
}

const SettingsChromeVoxSubpageElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

class SettingsChromeVoxSubpageElement extends
    SettingsChromeVoxSubpageElementBase {
  static get is() {
    return 'settings-chromevox-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Dropdown menu choices for capital strategy options.
       */
      capitalStrategyOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'announceCapitals',
              name: loadTimeData.getString('chromeVoxAnnounceCapitals'),
            },
            {
              value: 'increasePitch',
              name: loadTimeData.getString('chromeVoxIncreasePitch'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for number reading style options.
       */
      numberReadingStyleOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'asWords',
              name: loadTimeData.getString('chromeVoxAsWords'),
            },
            {
              value: 'asDigits',
              name: loadTimeData.getString('chromeVoxAsDigits'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for punctuation echo options.
       */
      punctuationEchoOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 0,
              name: loadTimeData.getString('chromeVoxNone'),
            },
            {
              value: 1,
              name: loadTimeData.getString('chromeVoxSome'),
            },
            {
              value: 2,
              name: loadTimeData.getString('chromeVoxAll'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for audio strategy options.
       */
      audioStrategyOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'audioNormal',
              name: loadTimeData.getString('chromeVoxAudioNormal'),
            },
            {
              value: 'audioDuck',
              name: loadTimeData.getString('chromeVoxAudioDuck'),
            },
            {
              value: 'audioSuspend',
              name: loadTimeData.getString('chromeVoxAudioSuspend'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for voice options.
       */
      voiceOptions_: {
        type: Array,
        value: [],
      },

      /**
       * Whether developer options is expanded.
       */
      developerOptionsExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [];
  }

  private route_: Route;
  private capitalStrategyOptions_: DropdownMenuOptionList;
  private numberReadingStyleOptions_: DropdownMenuOptionList;
  private punctuationEchoOptions_: DropdownMenuOptionList;
  private audioStrategyOptions_: DropdownMenuOptionList;
  private voiceOptions_: DropdownMenuOptionList;
  private chromeVoxBrowserProxy_: ChromeVoxSubpageBrowserProxy;

  // TODO(270619855): Add tests to verify these controls change their prefs.
  constructor() {
    super();

    this.chromeVoxBrowserProxy_ =
        ChromeVoxSubpageBrowserProxyImpl.getInstance();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_CHROMEVOX;
  }

  override ready() {
    super.ready();

    this.addWebUiListener(
        'all-voice-data-updated',
        (voices: TtsHandlerVoice[]) => this.populateVoiceList_(voices));
    this.chromeVoxBrowserProxy_.getAllTtsVoiceData();
    this.chromeVoxBrowserProxy_.refreshTtsVoices();
  }

  /**
   * Note: Overrides RouteOriginMixin implementation.
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route_) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * When usePitchChanges is toggled, we should update the preference value and
   * dropdown for capitalStrategy. (The capitalStrategy pref depends on the
   * value of usePitchChanges.)
   * TODO(b/270619855): Add test to verify correct dropdown state when toggling.
   */
  private onUsePitchChangesToggled_(event: Event): void {
    const usePitchChanges =
        (event.target as SettingsToggleButtonElement).checked;

    if (!usePitchChanges) {
      // Backup and disable capitalStrategy setting and set to announceCapitals.
      this.$.capitalStrategyDropdown.disabled = true;
      const capitalStrategy =
          this.getPref<string>('settings.a11y.chromevox.capital_strategy')
              .value;
      this.setPrefValue(
          'settings.a11y.chromevox.capital_strategy_backup', capitalStrategy);
      this.setPrefValue(
          'settings.a11y.chromevox.capital_strategy', 'announceCapitals');
      return;
    }

    // Restore original capitalStrategy setting.
    this.$.capitalStrategyDropdown.disabled = false;
    const capitalStrategyBackup =
        this.getPref<string>('settings.a11y.chromevox.capital_strategy_backup')
            .value;
    this.setPrefValue(
        'settings.a11y.chromevox.capital_strategy', capitalStrategyBackup);
  }

  /**
   * Populates the list of voices for the UI to use in display.
   */
  private populateVoiceList_(voices: TtsHandlerVoice[]): void {
    // TODO(b/271422242): voiceName can actually be omitted in the TTS engine.
    // We should generate a name in that case.
    voices.forEach(voice => voice.name = voice.name || '');
    voices.sort(function(a, b) {
      function score(voice: TtsHandlerVoice) {
        // Prefer Google tts voices over all others.
        if (voice.extensionId === GOOGLE_TTS_EXTENSION_ID) {
          return 4;
        }

        // Next, prefer Espeak tts voices.
        if (voice.extensionId === ESPEAK_TTS_EXTENSION_ID) {
          return 2;
        }

        // Finally, prefer local over remote voices.
        if (!voice.remote) {
          return 1;
        }
        return 0;
      }
      return score(b) - score(a);
    });

    this.voiceOptions_ = [
      {
        value: SYSTEM_VOICE,
        name: this.i18n('chromeVoxSystemVoice'),
      },
      ...voices.map(({name}) => ({value: name, name})),
    ];
  }

  private onTtsSettingsTap_(): void {
    Router.getInstance().navigateTo(
        routes.MANAGE_TTS_SETTINGS,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsChromeVoxSubpageElement.is]: SettingsChromeVoxSubpageElement;
  }
}

customElements.define(
    SettingsChromeVoxSubpageElement.is, SettingsChromeVoxSubpageElement);
