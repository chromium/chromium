// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The types of Hotword enable status without Dsp support.
 * @enum {number}
 */
const DspHotwordState = {
  DEFAULT_ON: 0,
  ALWAYS_ON: 1,
  OFF: 2,
};

/**
 * Indicates user's activity control consent status.
 *
 * Note: This should be kept in sync with ConsentStatus in
 * chromeos/services/assistant/public/cpp/assistant_prefs.h
 * @enum {number}
 */
const ConsentStatus = {
  // The status is unknown.
  kUnknown: 0,

  // The user accepted activity control access.
  kActivityControlAccepted: 1,

  // The user is not authorized to give consent.
  kUnauthorized: 2,

  // The user's consent information is not found. This is typically the case
  // when consent from the user has never been requested.
  kNotFound: 3,
};

/**
 * @fileoverview 'settings-google-assistant-page' is the settings page
 * containing Google Assistant settings.
 */
Polymer({
  is: 'settings-google-assistant-page',

  behaviors: [I18nBehavior, PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /** @private */
    isAssistantAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isAssistantAllowed');
      },
    },

    /** @private */
    shouldShowVoiceMatchSettings_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hotwordDspAvailable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('hotwordDspAvailable');
      },
    },

    /** @private */
    hotwordDropdownList_: {
      type: Array,
      value: function() {
        return [
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspRecommended'),
            value: DspHotwordState.DEFAULT_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspAlwaysOn'),
            value: DspHotwordState.ALWAYS_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspOff'),
            value: DspHotwordState.OFF
          }
        ];
      },
    },

    /** @private */
    hotwordEnforced_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    dspHotwordState_: {
      type: DspHotwordState,
    }
  },

  observers: [
    'onPrefsChanged_(prefs.settings.voice_interaction.hotword.enabled.value)',
    'onPrefsChanged_(prefs.settings.voice_interaction.hotword.always_on.value)',
    `onPrefsChanged_(
      prefs.settings.voice_interaction.activity_control.consent_status.value)`,
    'onPrefsChanged_(prefs.settings.assistant.disabled_by_policy.value)',
  ],

  /** @private {?settings.GoogleAssistantBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.GoogleAssistantBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('hotwordDeviceUpdated', (hasHotword) => {
      this.hotwordDspAvailable_ = hasHotword;
    });

    chrome.send('initializeGoogleAssistantPage');
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantOnOffLabel_: function(toggleValue) {
    return this.i18n(toggleValue ? 'toggleOn' : 'toggleOff');
  },

  /** @private */
  onGoogleAssistantSettingsTapped_: function() {
    this.browserProxy_.showGoogleAssistantSettings();
  },

  /** @private */
  onRetrainVoiceModelTapped_: function() {
    this.browserProxy_.retrainAssistantVoiceModel();
  },

  /** @private */
  onEnableHotwordChange_: function(event) {
    if (event.target.checked) {
      this.browserProxy_.syncVoiceModelStatus();
    }
  },

  /** @private */
  onDspHotwordStateChange_: function() {
    switch (Number(this.$$('#dsp-hotword-state').value)) {
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
  },

  /**
   * @param {number} state
   * @return {boolean}
   * @private
   */
  isDspHotwordStateMatch_: function(state) {
    return state == this.dspHotwordState_;
  },

  /** @private */
  onPrefsChanged_: function() {
    if (this.getPref('settings.assistant.disabled_by_policy.value')) {
      this.setPrefValue('settings.voice_interaction.enabled', false);
      return;
    }

    this.refreshDspHotwordState_();

    this.shouldShowVoiceMatchSettings_ =
        !loadTimeData.getBoolean('voiceMatchDisabled') &&
        this.getPref('settings.voice_interaction.hotword.enabled.value') &&
        (this.getPref(
          'settings.voice_interaction.activity_control.consent_status.value') ==
            ConsentStatus.kActivityControlAccepted);

    const hotwordEnabled =
        this.getPref('settings.voice_interaction.hotword.enabled');

    this.hotwordEnforced_ = hotwordEnabled.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  refreshDspHotwordState_: function() {
    if (!this.getPref('settings.voice_interaction.hotword.enabled.value')) {
      this.dspHotwordState_ = DspHotwordState.OFF;
    } else if (this.getPref(
                   'settings.voice_interaction.hotword.always_on.value')) {
      this.dspHotwordState_ = DspHotwordState.ALWAYS_ON;
    } else {
      this.dspHotwordState_ = DspHotwordState.DEFAULT_ON;
    }

    if (this.$$('#dsp-hotword-state')) {
      this.$$('#dsp-hotword-state').value = this.dspHotwordState_;
    }
  },
});
