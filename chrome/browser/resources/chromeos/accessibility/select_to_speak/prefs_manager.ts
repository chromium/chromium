// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {SelectToSpeakConstants} from './select_to_speak_constants.js';

/**
 * Manages getting and storing user preferences.
 */
export class PrefsManager {
  /** Please keep fields in alphabetical order. */

  private backgroundShadingEnabled_ = false;
  private color_ = '#da36e8';
  /**
   * Whether to allow enhanced network voices in Select-to-Speak. Unlike
   * |this.enhancedNetworkVoicesEnabled_|, which represents the user's
   * preference, |this.enhancedNetworkVoicesAllowed_| is set by admin via
   * policy. |this.enhancedNetworkVoicesAllowed_| does not override
   * |this.enhancedNetworkVoicesEnabled_| but changes
   * this.enhancedNetworkVoicesEnabled().
   */
  private enhancedNetworkVoicesAllowed_ = true;
  /**
   * A pref indicating whether the user enables the network voices. The pref
   * is synced to local storage as "enhancedNetworkVoices". Use
   * this.enhancedNetworkVoicesEnabled() to refer whether to enable the
   * network voices instead of using this pref directly.
   */
  private enhancedNetworkVoicesEnabled_ = false;
  private enhancedVoiceName_ = PrefsManager.DEFAULT_NETWORK_VOICE;
  private enhancedVoicesDialogShown_ = false;
  private extensionForVoice_: Map<string, string> = new Map();
  private highlightColor_ = '#5e9bff';
  private migrationInProgress_ = false;
  private navigationControlsEnabled_ = true;
  private speechRate_ = 1.0;
  private validVoiceNames_: Set<string> = new Set();
  private voiceNameFromLocale_: string|null = null;
  private voiceNameFromPrefs_: string|null = null;
  private wordHighlight_ = true;

  /** TODO(crbug.com/950391): Ask UX about the default value here. */
  private voiceSwitching_ = false;

  /**
   * Used by tests to wait for settings changes to be propagated.
   */
  private updateSettingsPrefsCallbackForTest_: (() => {})|null = null;

  constructor() {}

  /**
   * Get the list of TTS voices, and set the default voice if not already set.
   */
  private updateDefaultVoice_(): void {
    var uiLocale = chrome.i18n.getMessage('@@ui_locale');
    uiLocale = uiLocale.replace('_', '-').toLowerCase();

    chrome.tts.getVoices(voices => {
      this.validVoiceNames_ = new Set();

      if (voices.length === 0) {
        return;
      }

      voices.forEach(voice => {
        // TODO(b/270623046): voice.eventTypes may be undefined.
        if (!voice.eventTypes!.includes(chrome.tts.EventType.START) ||
            !voice.eventTypes!.includes(chrome.tts.EventType.END) ||
            !voice.eventTypes!.includes(chrome.tts.EventType.WORD) ||
            !voice.eventTypes!.includes(chrome.tts.EventType.CANCELLED)) {
          return;
        }

        if (voice.voiceName) {
          this.extensionForVoice_.set(voice.voiceName, voice.extensionId || '');
          if ((voice.extensionId !== PrefsManager.ENHANCED_TTS_EXTENSION_ID) &&
              !voice.remote) {
            // Don't consider network voices when computing default.
            this.validVoiceNames_.add(voice.voiceName);
          }
        }
      });

      voices.sort(function(a, b) {
        function score(voice: chrome.tts.TtsVoice): number {
          if (voice.lang === undefined) {
            return -1;
          }
          var lang = voice.lang.toLowerCase();
          var s = 0;
          if (lang === uiLocale) {
            s += 2;
          }
          if (lang.substr(0, 2) === uiLocale.substr(0, 2)) {
            s += 1;
          }
          return s;
        }
        return score(b) - score(a);
      });

      const firstVoiceName = voices[0].voiceName;
      if (firstVoiceName) {
        this.voiceNameFromLocale_ = firstVoiceName;
      }
    });
  }

  /**
   * Migrates Select-to-Speak rate and pitch settings to global Text-to-Speech
   * settings. This is a one-time migration that happens on upgrade to M70.
   * See http://crbug.com/866550.
   */
  private migrateToGlobalTtsSettings_(rateStr: string, pitchStr: string): void {
    if (this.migrationInProgress_) {
      return;
    }
    this.migrationInProgress_ = true;
    let stsRate = PrefsManager.DEFAULT_RATE;
    let stsPitch = PrefsManager.DEFAULT_PITCH;
    let globalRate = PrefsManager.DEFAULT_RATE;
    let globalPitch = PrefsManager.DEFAULT_PITCH;

    if (rateStr !== undefined) {
      stsRate = parseFloat(rateStr);
    }
    if (pitchStr !== undefined) {
      stsPitch = parseFloat(pitchStr);
    }
    // Get global prefs using promises so that we can receive both pitch and
    // rate before doing migration logic.
    const getPrefsPromises = [];
    getPrefsPromises.push(new Promise<void>((resolve, reject) => {
      chrome.settingsPrivate.getPref('settings.tts.speech_rate', pref => {
        if (pref === undefined) {
          reject();
        }
        globalRate = pref.value;
        resolve();
      });
    }));
    getPrefsPromises.push(new Promise<void>((resolve, reject) => {
      chrome.settingsPrivate.getPref('settings.tts.speech_pitch', pref => {
        if (pref === undefined) {
          reject();
        }
        globalPitch = pref.value;
        resolve();
      });
    }));
    Promise.all(getPrefsPromises)
        .then(
            () => {
              const stsOptionsModified =
                  stsRate !== PrefsManager.DEFAULT_RATE ||
                  stsPitch !== PrefsManager.DEFAULT_PITCH;
              const globalOptionsModified =
                  globalRate !== PrefsManager.DEFAULT_RATE ||
                  globalPitch !== PrefsManager.DEFAULT_PITCH;
              const optionsEqual =
                  stsRate === globalRate && stsPitch === globalPitch;
              if (optionsEqual) {
                // No need to write global prefs if all the prefs are the same
                // as defaults. Just remove STS rate and pitch.
                this.onTtsSettingsMigrationSuccess_();
                return;
              }
              if (stsOptionsModified && !globalOptionsModified) {
                // Set global prefs using promises so we can set both rate and
                // pitch successfully before removing the preferences from
                // chrome.storage.sync.
                const setPrefsPromises = [];
                setPrefsPromises.push(new Promise<void>((resolve, reject) => {
                  chrome.settingsPrivate.setPref(
                      'settings.tts.speech_rate', stsRate,
                      '' /* unused, see crbug.com/866161 */, success => {
                        if (success) {
                          resolve();
                        } else {
                          reject();
                        }
                      });
                }));
                setPrefsPromises.push(new Promise<void>((resolve, reject) => {
                  chrome.settingsPrivate.setPref(
                      'settings.tts.speech_pitch', stsPitch,
                      '' /* unused, see crbug.com/866161 */, success => {
                        if (success) {
                          resolve();
                        } else {
                          reject();
                        }
                      });
                }));
                Promise.all(setPrefsPromises)
                    .then(
                        () => this.onTtsSettingsMigrationSuccess_(), error => {
                          console.log(error);
                          this.migrationInProgress_ = false;
                        });
              } else if (globalOptionsModified) {
                // Global options were already modified, so STS will use global
                // settings regardless of whether STS was modified yet or not.
                this.onTtsSettingsMigrationSuccess_();
              }
            },
            error => {
              console.log(error);
              this.migrationInProgress_ = false;
            });
  }

  /**
   * When TTS settings are successfully migrated, removes rate and pitch from
   * chrome.storage.sync.
   */
  private onTtsSettingsMigrationSuccess_(): void {
    chrome.storage.sync.remove('rate');
    chrome.storage.sync.remove('pitch');
    this.migrationInProgress_ = false;
  }

  /**
   * Loads prefs and policy from chrome.settingsPrivate.
   */
  private updateSettingsPrefs_(prefs: chrome.settingsPrivate.PrefObject[]):
      void {
    for (const pref of prefs) {
      switch (pref.key) {
        case PrefsManager.VOICE_NAME_KEY:
          this.voiceNameFromPrefs_ = pref.value;
          break;
        case PrefsManager.SPEECH_RATE_KEY:
          this.speechRate_ = pref.value;
          break;
        case PrefsManager.WORD_HIGHLIGHT_KEY:
          this.wordHighlight_ = pref.value;
          break;
        case PrefsManager.HIGHLIGHT_COLOR_KEY:
          this.highlightColor_ = pref.value;
          break;
        case PrefsManager.BACKGROUND_SHADING_KEY:
          this.backgroundShadingEnabled_ = pref.value;
          break;
        case PrefsManager.NAVIGATION_CONTROLS_KEY:
          this.navigationControlsEnabled_ = pref.value;
          break;
        case PrefsManager.ENHANCED_NETWORK_VOICES_KEY:
          this.enhancedNetworkVoicesEnabled_ = pref.value;
          break;
        case PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY:
          this.enhancedVoicesDialogShown_ = pref.value;
          break;
        case PrefsManager.ENHANCED_VOICE_NAME_KEY:
          this.enhancedVoiceName_ = pref.value;
          break;
        case PrefsManager.ENHANCED_VOICES_POLICY_KEY:
          this.enhancedNetworkVoicesAllowed_ = pref.value;
          break;
        case PrefsManager.VOICE_SWITCHING_KEY:
          this.voiceSwitching_ = pref.value;
          break;
      }
    }
    if (this.updateSettingsPrefsCallbackForTest_) {
      this.updateSettingsPrefsCallbackForTest_();
    }
  }

  /**
   * Migrates prefs from chrome.storage to Chrome settings prefs. This will
   * enable us to move Select-to-speak options into the Chrome OS Settings app.
   * This should only occur once per pref, as we remove the chrome.storage pref
   * after we copy it over.
   */
  private migrateStorageToSettingsPref_(
      storagePrefName: string, settingsPrefName: string, value: any): void {
    chrome.settingsPrivate.setPref(settingsPrefName, value);
    chrome.storage.sync.remove(storagePrefName);
  }

  /**
   * Loads prefs from chrome.storage and sets values in settings prefs if
   * necessary.
   */
  private async updateStoragePrefs_(): Promise<void> {
    const prefs: Record<string, any> = await new Promise(
        resolve => chrome.storage.sync.get(
            [
              'voice',
              'rate',
              'pitch',
              'wordHighlight',
              'highlightColor',
              'backgroundShading',
              'navigationControls',
              'enhancedNetworkVoices',
              'enhancedVoicesDialogShown',
              'enhancedVoiceName',
              'voiceSwitching',
            ],
            resolve));

    if (prefs['voice']) {
      this.voiceNameFromPrefs_ = prefs['voice'];
      this.migrateStorageToSettingsPref_(
          'voice', PrefsManager.VOICE_NAME_KEY, this.voiceNameFromPrefs_);
    }
    if (prefs['wordHighlight'] !== undefined) {
      this.wordHighlight_ = prefs['wordHighlight'];
      this.migrateStorageToSettingsPref_(
          'wordHighlight', PrefsManager.WORD_HIGHLIGHT_KEY,
          this.wordHighlight_);
    }
    if (prefs['highlightColor']) {
      this.highlightColor_ = prefs['highlightColor'];
      this.migrateStorageToSettingsPref_(
          'highlightColor', PrefsManager.HIGHLIGHT_COLOR_KEY,
          this.highlightColor_);
    }
    if (prefs['backgroundShading'] !== undefined) {
      this.backgroundShadingEnabled_ = prefs['backgroundShading'];
      this.migrateStorageToSettingsPref_(
          'backgroundShading', PrefsManager.BACKGROUND_SHADING_KEY,
          this.backgroundShadingEnabled_);
    }
    if (prefs['navigationControls'] !== undefined) {
      this.navigationControlsEnabled_ = prefs['navigationControls'];
      this.migrateStorageToSettingsPref_(
          'navigationControls', PrefsManager.NAVIGATION_CONTROLS_KEY,
          this.navigationControlsEnabled_);
    }
    if (prefs['enhancedNetworkVoices'] !== undefined) {
      this.enhancedNetworkVoicesEnabled_ = prefs['enhancedNetworkVoices'];
      this.migrateStorageToSettingsPref_(
          'enhancedNetworkVoices', PrefsManager.ENHANCED_NETWORK_VOICES_KEY,
          this.enhancedNetworkVoicesEnabled_);
    }
    if (prefs['enhancedVoicesDialogShown'] !== undefined) {
      this.enhancedVoicesDialogShown_ = prefs['enhancedVoicesDialogShown'];
      this.migrateStorageToSettingsPref_(
          'enhancedVoicesDialogShown',
          PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY,
          this.enhancedVoicesDialogShown_);
    }
    if (prefs['enhancedVoiceName'] !== undefined) {
      this.enhancedVoiceName_ = prefs['enhancedVoiceName'];
      this.migrateStorageToSettingsPref_(
          'enhancedVoiceName', PrefsManager.ENHANCED_VOICE_NAME_KEY,
          this.enhancedVoiceName_);
    }
    if (prefs['voiceSwitching'] !== undefined) {
      this.voiceSwitching_ = prefs['voiceSwitching'];
      this.migrateStorageToSettingsPref_(
          'voiceSwitching', PrefsManager.VOICE_SWITCHING_KEY,
          this.voiceSwitching_);
    }
    if (prefs['rate'] && prefs['pitch']) {
      // Removes 'rate' and 'pitch' prefs after migrating data to global
      // TTS settings if appropriate.
      this.migrateToGlobalTtsSettings_(prefs['rate'], prefs['pitch']);
    }
  }

  /**
   * Loads prefs and policy from chrome.storage and chrome.settingsPrivate,
   * sets default values if necessary, and registers a listener to update prefs
   * and policy when they change.
   */
  async initPreferences(): Promise<void> {
    // Migrate from storage prefs if necessary.
    await this.updateStoragePrefs_();

    // Initialize prefs from settings.
    const settingsPrefs: chrome.settingsPrivate.PrefObject[] =
        await new Promise(
            resolve => chrome.settingsPrivate.getAllPrefs(resolve));
    this.updateSettingsPrefs_(settingsPrefs);

    chrome.settingsPrivate.onPrefsChanged.addListener(
        prefs => this.updateSettingsPrefs_(prefs));
    chrome.storage.onChanged.addListener(() => this.updateStoragePrefs_());

    this.updateDefaultVoice_();
    chrome.tts.onVoicesChanged.addListener(() => {
      this.updateDefaultVoice_();
    });
  }

  /**
   * Get the voice name of the user's preferred local voice.
   * @return Name of preferred local voice.
   */
  getLocalVoice(): string|undefined {
    // To use the default (system) voice: don't specify options['voiceName'].
    if (this.voiceNameFromPrefs_ === PrefsManager.SYSTEM_VOICE) {
      return undefined;
    }

    // Pick the voice name from prefs first, or the one that matches
    // the locale next, but don't pick a voice that isn't currently
    // loaded. If no voices are found, leave the voiceName option
    // unset to let the browser try to route the speech request
    // anyway if possible.
    if (this.voiceNameFromPrefs_ &&
        this.validVoiceNames_.has(this.voiceNameFromPrefs_)) {
      return this.voiceNameFromPrefs_;
    } else if (
        this.voiceNameFromLocale_ &&
        this.validVoiceNames_.has(this.voiceNameFromLocale_)) {
      return this.voiceNameFromLocale_;
    }

    return undefined;
  }

  /**
   * Generates the basic speech options for Select-to-Speak based on user
   * preferences. Call for each chrome.tts.speak.
   */
  getSpeechOptions(voiceSwitchingData:
                       SelectToSpeakConstants.VoiceSwitchingData|
                   null): chrome.tts.TtsOptions {
    const options: chrome.tts.TtsOptions = {};
    const data: SelectToSpeakConstants.VoiceSwitchingData =
        voiceSwitchingData || {
          language: undefined,
          useVoiceSwitching: false,
        };
    const useEnhancedVoices =
        this.enhancedNetworkVoicesEnabled() && navigator.onLine;

    if (useEnhancedVoices) {
      options['voiceName'] = this.enhancedVoiceName_;
    } else {
      const useVoiceSwitching = data.useVoiceSwitching;
      const language = data.language;
      // If `useVoiceSwitching` is true, then we should omit `voiceName` from
      // options and let the TTS engine pick the right voice for the language.
      const localVoice = useVoiceSwitching ? undefined : this.getLocalVoice();
      if (localVoice !== undefined) {
        options['voiceName'] = localVoice;
      }
      if (language !== undefined) {
        options['lang'] = language;
      }
    }
    return options;
  }

  /**
   * Returns extension ID of the TTS engine for given voice name.
   * @param voiceName Voice name specified in TTS options
   * @return extension ID of TTS engine
   */
  ttsExtensionForVoice(voiceName: string): string {
    return this.extensionForVoice_.get(voiceName) || '';
  }

  /**
   * Checks if the voice is an enhanced network TTS voice.
   * @returns {boolean} True if the voice is an enhanced network TTS voice.
   */
  isNetworkVoice(voiceName: string): boolean {
    return this.ttsExtensionForVoice(voiceName) ===
        PrefsManager.ENHANCED_TTS_EXTENSION_ID;
  }
  /**
   * Gets the user's word highlighting enabled preference.
   * @return True if word highlighting is enabled.
   */
  wordHighlightingEnabled(): boolean {
    return this.wordHighlight_;
  }

  /**
   * Gets the user's word highlighting color preference.
   * @return Highlight color.
   */
  highlightColor(): string {
    return this.highlightColor_;
  }

  /**
   * Gets the focus ring color. This is not currently a user preference but it
   * could be in the future; stored here for similarity to highlight color.
   * @return Highlight color.
   */
  focusRingColor(): string {
    return this.color_;
  }

  /**
   * Gets the user's focus ring background color. If the user disabled greying
   * out the background, alpha will be set to fully transparent.
   * @return True if the background shade should be drawn.
   */
  backgroundShadingEnabled(): boolean {
    return this.backgroundShadingEnabled_;
  }

  /**
   * Gets the user's preference for showing navigation controls that allow them
   * to navigate to next/previous sentences, paragraphs, and more.
   * @return True if navigation controls should be shown when STS is
   *     active.
   */
  navigationControlsEnabled(): boolean {
    return this.navigationControlsEnabled_;
  }

  /**
   * Gets the user's preference for speech rate.
   * @return Current TTS speech rate.
   */
  speechRate(): number {
    return this.speechRate_;
  }

  /**
   * Gets the user's preference for whether enhanced network TTS voices are
   * enabled. Always returns false if the policy disallows the feature.
   * @return True if enhanced TTS voices are enabled.
   */
  enhancedNetworkVoicesEnabled(): boolean {
    return this.enhancedNetworkVoicesAllowed_ ?
        this.enhancedNetworkVoicesEnabled_ :
        false;
  }

  /**
   * Gets the admin's policy for whether enhanced network TTS voices are
   * allowed.
   * @return True if enhanced TTS voices are allowed.
   */
  enhancedNetworkVoicesAllowed(): boolean {
    return this.enhancedNetworkVoicesAllowed_;
  }

  /**
   * Gets whether the initial popup authorizing enhanced network voices has been
   * shown to the user or not.
   *
   * @returns True if the initial popup dialog has been shown already.
   */
  enhancedVoicesDialogShown(): boolean {
    return this.enhancedVoicesDialogShown_;
  }

  /**
   * Sets whether enhanced network voices are enabled or not from initial popup.
   * @param enabled Specifies if the user enabled enhanced voices in
   *     the popup.
   */
  setEnhancedNetworkVoicesFromDialog(enabled: boolean): void {
    if (enabled === undefined) {
      return;
    }
    this.enhancedNetworkVoicesEnabled_ = enabled;
    chrome.settingsPrivate.setPref(
        PrefsManager.ENHANCED_NETWORK_VOICES_KEY,
        this.enhancedNetworkVoicesEnabled_);

    this.enhancedVoicesDialogShown_ = true;
    chrome.settingsPrivate.setPref(
        PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY,
        this.enhancedVoicesDialogShown_);

    if (!this.enhancedNetworkVoicesAllowed_) {
      console.warn(
          'Network voices dialog was shown when the policy disallows it.');
    }
  }

  /**
   * Gets the user's preference for whether automatic voice switching between
   * languages is enabled.
   */
  voiceSwitchingEnabled(): boolean {
    return this.voiceSwitching_;
  }
}

export namespace PrefsManager {
  /**
   * Constant used as the value for a menu option representing the current
   * device language.
   */
  export const USE_DEVICE_LANGUAGE = 'select_to_speak_device_language';

  /**
   * Constant representing the system TTS voice.
   */
  export const SYSTEM_VOICE = 'select_to_speak_system_voice';

  /**
   * Constant representing the voice name for the default (server-selected)
   * network TTS voice.
   */
  export const DEFAULT_NETWORK_VOICE = 'default-wavenet';

  /**
   * Extension ID of the enhanced network TTS voices extension.
   */
  export const ENHANCED_TTS_EXTENSION_ID = 'jacnkoglebceckolkoapelihnglgaicd';

  /**
   * Extension ID of the Google TTS voices extension.
   */
  export const GOOGLE_TTS_EXTENSION_ID = 'gjjabgpgjpampikjhjpfhneeoapjbjaf';

  /**
   * Extension ID of the eSpeak TTS voices extension.
   */
  export const ESPEAK_EXTENSION_ID = 'dakbfdmgjiabojdgbiljlhgjbokobjpg';

  /**
   * Default speech rate for both Select-to-Speak and global prefs.
   */
  export const DEFAULT_RATE = 1.0;

  /**
   * Default speech pitch for both Select-to-Speak and global prefs.
   */
  export const DEFAULT_PITCH = 1.0;

  /**
   * Settings key for the pref for whether to shade the background area of the
   * screen (where text isn't currently being spoken).
   */
  export const BACKGROUND_SHADING_KEY =
      'settings.a11y.select_to_speak_background_shading';

  /**
   * Settings key for the pref for whether enhanced network TTS voices are
   * enabled.
   */
  export const ENHANCED_NETWORK_VOICES_KEY =
      'settings.a11y.select_to_speak_enhanced_network_voices';

  /**
   * Settings key for the pref indicating the user's enhanced voice preference.
   */
  export const ENHANCED_VOICE_NAME_KEY =
      'settings.a11y.select_to_speak_enhanced_voice_name';

  /**
   * Settings key for the pref indicating whether initial popup authorizing
   * enhanced network voices has been shown to the user or not.
   */
  export const ENHANCED_VOICES_DIALOG_SHOWN_KEY =
      'settings.a11y.select_to_speak_enhanced_voices_dialog_shown';

  /**
   * Settings key for the policy indicating whether to allow enhanced network
   * voices.
   */
  export const ENHANCED_VOICES_POLICY_KEY =
      'settings.a11y.enhanced_network_voices_in_select_to_speak_allowed';

  /**
   * Settings key for the pref indicating the user's word highlighting color
   * preference.
   */
  export const HIGHLIGHT_COLOR_KEY =
      'settings.a11y.select_to_speak_highlight_color';

  /**
   * Settings key for the pref for showing navigation controls.
   */
  export const NAVIGATION_CONTROLS_KEY =
      'settings.a11y.select_to_speak_navigation_controls';

  /**
   * Settings key for the pref indicating the user's system-wide preference TTS
   * speech rate.
   */
  export const SPEECH_RATE_KEY = 'settings.tts.speech_rate';

  /**
   * Settings key for the pref indicating the user's voice preference.
   */
  export const VOICE_NAME_KEY = 'settings.a11y.select_to_speak_voice_name';

  /**
   * Settings key for the pref for enabling automatic voice switching between
   * languages.
   */
  export const VOICE_SWITCHING_KEY =
      'settings.a11y.select_to_speak_voice_switching';

  /**
   * Settings key for the pref indicating whether to enable word highlighting.
   */
  export const WORD_HIGHLIGHT_KEY =
      'settings.a11y.select_to_speak_word_highlight';
}

TestImportManager.exportForTesting(PrefsManager);
