// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages getting and storing user preferences.
 * @constructor
 */
let PrefsManager = function() {
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
  this.color_ = '#f73a98';

  /** @private {string} */
  this.highlightColor_ = '#5e9bff';

  /** @private {boolean} */
  this.migrationInProgress_ = false;
};

/**
 * Constant representing the system TTS voice.
 * @type {string}
 * @public
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

/**
 * Get the list of TTS voices, and set the default voice if not already set.
 * @private
 */
PrefsManager.prototype.updateDefaultVoice_ = function() {
  var uiLocale = chrome.i18n.getMessage('@@ui_locale');
  uiLocale = uiLocale.replace('_', '-').toLowerCase();

  chrome.tts.getVoices((voices) => {
    this.validVoiceNames_ = new Set();

    if (voices.length == 0) {
      return;
    }

    voices.forEach((voice) => {
      if (!voice.eventTypes.includes('start') ||
          !voice.eventTypes.includes('end') ||
          !voice.eventTypes.includes('word') ||
          !voice.eventTypes.includes('cancelled')) {
        return;
      }
      this.validVoiceNames_.add(voice.voiceName);
    });

    voices.sort(function(a, b) {
      function score(voice) {
        if (voice.lang === undefined) {
          return -1;
        }
        var lang = voice.lang.toLowerCase();
        var s = 0;
        if (lang == uiLocale) {
          s += 2;
        }
        if (lang.substr(0, 2) == uiLocale.substr(0, 2)) {
          s += 1;
        }
        return s;
      }
      return score(b) - score(a);
    });

    this.voiceNameFromLocale_ = voices[0].voiceName;

    chrome.storage.sync.get(['voice'], (prefs) => {
      if (!prefs['voice']) {
        chrome.storage.sync.set({'voice': PrefsManager.SYSTEM_VOICE});
      }
    });
  });
};

/**
 * Migrates Select-to-Speak rate and pitch settings to global Text-to-Speech
 * settings. This is a one-time migration that happens on upgrade to M70.
 * See http://crbug.com/866550.
 * @param {string} rateStr
 * @param {string} pitchStr
 * @private
 */
PrefsManager.prototype.migrateToGlobalTtsSettings_ = function(
    rateStr, pitchStr) {
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
  let getPrefsPromises = [];
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
            let stsOptionsModified = stsRate != PrefsManager.DEFAULT_RATE ||
                stsPitch != PrefsManager.DEFAULT_PITCH;
            let globalOptionsModified =
                globalRate != PrefsManager.DEFAULT_RATE ||
                globalPitch != PrefsManager.DEFAULT_PITCH;
            let optionsEqual = stsRate == globalRate && stsPitch == globalPitch;
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
              let setPrefsPromises = [];
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
};

/**
 * When TTS settings are successfully migrated, removes rate and pitch from
 * chrome.storage.sync.
 * @private
 */
PrefsManager.prototype.onTtsSettingsMigrationSuccess_ = function() {
  chrome.storage.sync.remove('rate');
  chrome.storage.sync.remove('pitch');
  this.migrationInProgress_ = false;
};

/**
 * Loads preferences from chrome.storage, sets default values if
 * necessary, and registers a listener to update prefs when they
 * change.
 * @public
 */
PrefsManager.prototype.initPreferences = function() {
  var updatePrefs = () => {
    chrome.storage.sync.get(
        ['voice', 'rate', 'pitch', 'wordHighlight', 'highlightColor'],
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
};

/**
 * Generates the basic speech options for Select-to-Speak based on user
 * preferences. Call for each chrome.tts.speak.
 * @return {!TtsOptions} options The TTS options.
 * @public
 */
PrefsManager.prototype.speechOptions = function() {
  let options = {enqueue: true};

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
};

/**
 * Gets the user's word highlighting enabled preference.
 * @return {boolean} True if word highlighting is enabled.
 * @public
 */
PrefsManager.prototype.wordHighlightingEnabled = function() {
  return this.wordHighlight_;
};

/**
 * Gets the user's word highlighting color preference.
 * @return {string} Highlight color.
 * @public
 */
PrefsManager.prototype.highlightColor = function() {
  return this.highlightColor_;
};

/**
 * Gets the focus ring color. This is not currently a user preference but it
 * could be in the future; stored here for similarity to highlight color.
 * @return {string} Highlight color.
 * @public
 */
PrefsManager.prototype.focusRingColor = function() {
  return this.color_;
};
