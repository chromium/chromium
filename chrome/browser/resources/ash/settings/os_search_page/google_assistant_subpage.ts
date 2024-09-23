// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-google-assistant-subpage' is the settings page
 * containing Google Assistant settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '/shared/settings/prefs/prefs.js';
import '/shared/settings/prefs/pref_util.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast, castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {GoogleAssistantBrowserProxy, GoogleAssistantBrowserProxyImpl} from './google_assistant_browser_proxy.js';
import {getTemplate} from './google_assistant_subpage.html.js';

/**
 * The types of Hotword enable status without Dsp support.
 */
export enum DspHotwordState {
  DEFAULT_ON = 0,
  ALWAYS_ON = 1,
  OFF = 2,
}

/**
 * Indicates user's activity control consent status.
 *
 * Note: This should be kept in sync with ConsentStatus in
 * chromeos/ash/services/assistant/public/cpp/assistant_prefs.h
 */
export enum ConsentStatus {
  // The status is unknown.
  UNKNOWN = 0,

  // The user accepted activity control access.
  ACTIVITY_CONTROL_ACCEPTED = 1,

  // The user is not authorized to give consent.
  UNAUTHORIZED = 2,

  // The user's consent information is not found. This is typically the case
  // when consent from the user has never been requested.
  NOT_FOUND = 3,
}

const SettingsGoogleAssistantSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsGoogleAssistantSubpageElement extends
    SettingsGoogleAssistantSubpageElementBase {
  static get is() {
    return 'settings-google-assistant-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shouldShowVoiceMatchSettings_: {
        type: Boolean,
        value: false,
      },

      hotwordDspAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('hotwordDspAvailable');
        },
      },

      hotwordDropdownList_: {
        type: Array,
        value() {
          return [
            {
              name: loadTimeData.getString(
                  'googleAssistantEnableHotwordWithoutDspRecommended'),
              value: DspHotwordState.DEFAULT_ON,
            },
            {
              name: loadTimeData.getString(
                  'googleAssistantEnableHotwordWithoutDspAlwaysOn'),
              value: DspHotwordState.ALWAYS_ON,
            },
            {
              name: loadTimeData.getString(
                  'googleAssistantEnableHotwordWithoutDspOff'),
              value: DspHotwordState.OFF,
            },
          ];
        },
      },

      hotwordEnforced_: {
        type: Boolean,
        value: false,
      },

      hotwordEnforcedForChild_: {
        type: Boolean,
        value: false,
      },

      dspHotwordState_: Number,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kAssistantOnOff,
          Setting.kAssistantRelatedInfo,
          Setting.kAssistantOkGoogle,
          Setting.kAssistantNotifications,
          Setting.kAssistantVoiceInput,
          Setting.kTrainAssistantVoiceModel,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'onPrefsChanged_(' +
          'prefs.settings.voice_interaction.hotword.enabled.value, ' +
          'prefs.settings.voice_interaction.hotword.always_on.value, ' +
          'prefs.settings.voice_interaction.activity_control.consent_status' +
          '.value, ' +
          'prefs.settings.assistant.disabled_by_policy.value)',
    ];
  }

  private browserProxy_: GoogleAssistantBrowserProxy;
  private dspHotwordState_: DspHotwordState;
  private hotwordDspAvailable_: boolean;
  private hotwordEnforced_: boolean;
  private hotwordEnforcedForChild_: boolean;
  private shouldShowVoiceMatchSettings_: boolean;

  constructor() {
    super();

    this.browserProxy_ = GoogleAssistantBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener('hotwordDeviceUpdated', (hasHotword: boolean) => {
      this.hotwordDspAvailable_ = hasHotword;
    });

    chrome.send('initializeGoogleAssistantPage');
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route): void {
    // Does not apply to this page.
    if (route !== routes.GOOGLE_ASSISTANT) {
      return;
    }

    this.attemptDeepLink();
  }

  private getAssistantOnOffLabel_(toggleValue: boolean): string {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantOn' : 'searchGoogleAssistantOff');
  }

  private onGoogleAssistantSettingsClick_(): void {
    this.browserProxy_.showGoogleAssistantSettings();
  }

  private onRetrainVoiceModelClick_(): void {
    this.browserProxy_.retrainAssistantVoiceModel();
    recordSettingChange(Setting.kTrainAssistantVoiceModel);
  }

  private onEnableHotwordChange_(event: Event): void {
    const target = cast(event.target, SettingsToggleButtonElement);
    if (target.checked) {
      this.browserProxy_.syncVoiceModelStatus();
    }
  }

  private onDspHotwordStateChange_(): void {
    const dspHotwordStateEl =
        castExists(this.shadowRoot!.querySelector<HTMLSelectElement>(
            '#dsp-hotword-state'));

    switch (Number(dspHotwordStateEl.value)) {
      case DspHotwordState.DEFAULT_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        this.browserProxy_.syncVoiceModelStatus();
        break;
      case DspHotwordState.ALWAYS_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue('settings.voice_interaction.hotword.always_on', true);
        this.browserProxy_.syncVoiceModelStatus();
        break;
      case DspHotwordState.OFF:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', false);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        break;
      default:
        console.error('Invalid Dsp hotword settings state');
    }
  }

  private isDspHotwordStateMatch_(state: number): boolean {
    return state === this.dspHotwordState_;
  }

  private onPrefsChanged_(): void {
    if (this.getPref('settings.assistant.disabled_by_policy').value) {
      this.setPrefValue('settings.voice_interaction.enabled', false);
      return;
    }

    this.refreshDspHotwordState_();

    this.shouldShowVoiceMatchSettings_ =
        !loadTimeData.getBoolean('voiceMatchDisabled') &&
        !!this.getPref('settings.voice_interaction.hotword.enabled').value;

    const hotwordEnabled =
        this.getPref('settings.voice_interaction.hotword.enabled');

    this.hotwordEnforced_ = hotwordEnabled.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    this.hotwordEnforcedForChild_ = this.hotwordEnforced_ &&
        hotwordEnabled.controlledBy ===
            chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION;
  }

  private refreshDspHotwordState_(): void {
    if (!this.getPref('settings.voice_interaction.hotword.enabled').value) {
      this.dspHotwordState_ = DspHotwordState.OFF;
    } else if (this.getPref('settings.voice_interaction.hotword.always_on')
                   .value) {
      this.dspHotwordState_ = DspHotwordState.ALWAYS_ON;
    } else {
      this.dspHotwordState_ = DspHotwordState.DEFAULT_ON;
    }

    const dspHotwordStateEl =
        this.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    if (dspHotwordStateEl) {
      dspHotwordStateEl.value = String(this.dspHotwordState_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-google-assistant-subpage': SettingsGoogleAssistantSubpageElement;
  }
}

customElements.define(
    SettingsGoogleAssistantSubpageElement.is,
    SettingsGoogleAssistantSubpageElement);
