// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages getting and storing user preferences.
 */
export class PrefsManager {
  constructor() {
    /** @private {?string} */
    this.voiceNameFromPrefs_ = null;

    /** @private {?string} */
    this.voiceNameFromLocale_ = null;

    /** @private {Set<string>} */
    this.validVoiceNames_ = new Set();

    /** @private {number} */
    this.speechRate_ = 1.0;

    /** @private {number} */
    this.speechPitch_ = 1.0;

    /** @private {boolean} */
    this.wordHighlight_ = true;

    /** @const {string} */
    this.color_ = '#da36e8';

    /** @private {string} */
    this.highlightColor_ = '#5e9bff';

    /** @private {boolean} */
    this.migrationInProgress_ = false;

    /** @private {boolean} */
    this.backgroundShadingEnabled_ = false;

    /** @private {boolean} */
    this.navigationControlsEnabled_ = true;
  }

  /**
   * Get the list of TTS voices, and set the default voice if not already set.
   * @private
   */
  updateDefaultVoice_() {
    var uiLocale = chrome.i18n.getMessage('@@ui_locale');
    uiLocale = uiLocale.replace('_', '-').toLowerCase();

    chrome.tts.getVoices((voices) => {
      this.validVoiceNames_ = new Set();

      if (voices.length === 0) {
        return;
      }

      voices.forEach((voice) => {
        if (!voice.eventTypes.includes(chrome.tts.EventType.START) ||
            !voice.eventTypes.includes(chrome.tts.EventType.END) ||
            !voice.eventTypes.includes(chrome.tts.EventType.WORD) ||
            !voice.eventTypes.includes(chrome.tts.EventType.CANCELLED)) {
          return;
        }
        if (voice.voiceName) {
          this.validVoiceNames_.add(voice.voiceName);
        }
      });

      voices.sort(function(a, b) {
        function score(voice) {
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

      chrome.storage.sync.get(['voice'], (prefs) => {
        if (!prefs['voice']) {
          chrome.storage.sync.set({'voice': PrefsManager.SYSTEM_VOICE});
        }
      });
    });
  }

  /**
   * Migrates Select-to-Speak rate and pitch settings to global Text-to-Speech
   * settings. This is a one-time migration that happens on upgrade to M70.
   * See http://crbug.com/866550.
   * @param {string} rateStr
   * @param {string} pitchStr
   * @private
   */
  migrateToGlobalTtsSettings_(rateStr, pitchStr) {
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
    getPrefsPromises.push(new Promise((resolve, reject) => {
      chrome.settingsPrivate.getPref('settings.tts.speech_rate', (pref) => {
        if (pref === undefined) {
          reject();
        }
        globalRate = pref.value;
        resolve();
      });
    }));
    getPrefsPromises.push(new Promise((resolve, reject) => {
      chrome.settingsPrivate.getPref('settings.tts.speech_pitch', (pref) => {
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
                setPrefsPromises.push(new Promise((resolve, reject) => {
                  chrome.settingsPrivate.setPref(
                      'settings.tts.speech_rate', stsRate,
                      '' /* unused, see crbug.com/866161 */, (success) => {
                        if (success) {
                          resolve();
                        } else {
                          reject();
                        }
                      });
                }));
                setPrefsPromises.push(new Promise((resolve, reject) => {
                  chrome.settingsPrivate.setPref(
                      'settings.tts.speech_pitch', stsPitch,
                      '' /* unused, see crbug.com/866161 */, (success) => {
                        if (success) {
                          resolve();
                        } else {
                          reject();
                        }
                      });
                }));
                Promise.all(setPrefsPromises)
                    .then(
                        this.onTtsSettingsMigrationSuccess_.bind(this),
                        (error) => {
                          console.log(error);
                          this.migrationInProgress_ = false;
                        });
              } else if (globalOptionsModified) {
                // Global options were already modified, so STS will use global
                // settings regardless of whether STS was modified yet or not.
                this.onTtsSettingsMigrationSuccess_();
              }
            },
            (error) => {
              console.log(error);
              this.migrationInProgress_ = false;
            });
  }

  /**
   * When TTS settings are successfully migrated, removes rate and pitch from
   * chrome.storage.sync.
   * @private
   */
  onTtsSettingsMigrationSuccess_() {
    chrome.storage.sync.remove('rate');
    chrome.storage.sync.remove('pitch');
    this.migrationInProgress_ = false;
  }

  /**
   * Loads preferences from chrome.storage, sets default values if
   * necessary, and registers a listener to update prefs when they
   * change.
   */
  initPreferences() {
    var updatePrefs = () => {
      chrome.storage.sync.get(
          [
            'voice', 'rate', 'pitch', 'wordHighlight', 'highlightColor',
            'backgroundShading', 'navigationControls'
          ],
          (prefs) => {
            if (prefs['voice']) {
              this.voiceNameFromPrefs_ = prefs['voice'];
            }
            if (prefs['wordHighlight'] !== undefined) {
              this.wordHighlight_ = prefs['wordHighlight'];
            } else {
              chrome.storage.sync.set({'wordHighlight': this.wordHighlight_});
            }
            if (prefs['highlightColor']) {
              this.highlightColor_ = prefs['highlightColor'];
            } else {
              chrome.storage.sync.set({'highlightColor': this.highlightColor_});
            }
            if (prefs['backgroundShading'] !== undefined) {
              this.backgroundShadingEnabled_ = prefs['backgroundShading'];
            } else {
              chrome.storage.sync.set(
                  {'backgroundShading': this.backgroundShadingEnabled_});
            }
            if (prefs['navigationControls'] !== undefined) {
              this.navigationControlsEnabled_ = prefs['navigationControls'];
            } else {
              chrome.storage.sync.set(
                  {'navigationControls': this.navigationControlsEnabled_});
            }
            if (prefs['rate'] && prefs['pitch']) {
              // Removes 'rate' and 'pitch' prefs after migrating data to global
              // TTS settings if appropriate.
              this.migrateToGlobalTtsSettings_(prefs['rate'], prefs['pitch']);
            }
          });
    };

    updatePrefs();
    chrome.storage.onChanged.addListener(updatePrefs);

    this.updateDefaultVoice_();
    window.speechSynthesis.onvoiceschanged = () => {
      this.updateDefaultVoice_();
    };
  }

  /**
   * Generates the basic speech options for Select-to-Speak based on user
   * preferences. Call for each chrome.tts.speak.
   * @return {!chrome.tts.TtsOptions} options The TTS options.
   */
  speechOptions() {
    const options = /** @type {!chrome.tts.TtsOptions} */ ({});

    // To use the default (system) voice: don't specify options['voiceName'].
    if (this.voiceNameFromPrefs_ === PrefsManager.SYSTEM_VOICE) {
      return options;
    }

    // Pick the voice name from prefs first, or the one that matches
    // the locale next, but don't pick a voice that isn't currently
    // loaded. If no voices are found, leave the voiceName option
    // unset to let the browser try to route the speech request
    // anyway if possible.
    var valid = '';
    this.validVoiceNames_.forEach(function(voiceName) {
      if (valid) {
        valid += ',';
      }
      valid += voiceName;
    });
    if (this.voiceNameFromPrefs_ &&
        this.validVoiceNames_.has(this.voiceNameFromPrefs_)) {
      options['voiceName'] = this.voiceNameFromPrefs_;
    } else if (
        this.voiceNameFromLocale_ &&
        this.validVoiceNames_.has(this.voiceNameFromLocale_)) {
      options['voiceName'] = this.voiceNameFromLocale_;
    }
    return options;
  }

  /**
   * Gets the user's word highlighting enabled preference.
   * @return {boolean} True if word highlighting is enabled.
   */
  wordHighlightingEnabled() {
    return this.wordHighlight_;
  }

  /**
   * Gets the user's word highlighting color preference.
   * @return {string} Highlight color.
   */
  highlightColor() {
    return this.highlightColor_;
  }

  /**
   * Gets the focus ring color. This is not currently a user preference but it
   * could be in the future; stored here for similarity to highlight color.
   * @return {string} Highlight color.
   */
  focusRingColor() {
    return this.color_;
  }

  /**
   * Gets the user's focus ring background color. If the user disabled greying
   * out the background, alpha will be set to fully transparent.
   * @return {boolean} True if the background shade should be drawn.
   */
  backgroundShadingEnabled() {
    return this.backgroundShadingEnabled_;
  }

  /**
   * Gets the user's preference for showing navigation controls that allow them
   * to navigate to next/previous sentences, paragraphs, and more.
   * @return {boolean} True if navigation controls should be shown when STS is
   *     active.
   */
  navigationControlsEnabled() {
    return this.navigationControlsEnabled_;
  }
}

/**
 * Constant representing the system TTS voice.
 * @type {string}
 */
PrefsManager.SYSTEM_VOICE = 'select_to_speak_system_voice';

/**
 * Default speech rate for both Select-to-Speak and global prefs.
 * @type {number}
 */
PrefsManager.DEFAULT_RATE = 1.0;

/**
 * Default speech pitch for both Select-to-Speak and global prefs.
 * @type {number}
 */
PrefsManager.DEFAULT_PITCH = 1.0;
