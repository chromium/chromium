// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This is a component extension that implements a text-to-speech (TTS)
 * engine powered by Google's speech synthesis API.
 *
 * This is an "event page", so it's not loaded when the API isn't being used,
 * and doesn't waste resources. When a web page or web app makes a speech
 * request and the parameters match one of the voices in this extension's
 * manifest, it makes a request to Google's API using Chrome's private key
 * and plays the resulting speech using HTML5 audio.
 */

/**
 * The main class for this extension. Adds listeners to
 * chrome.ttsEngine.onSpeak and chrome.ttsEngine.onStop and implements
 * them using Google's speech synthesis API.
 * @constructor
 */
function TtsExtension() {}

TtsExtension.prototype = {
  /**
   * The url prefix of the speech server, including static query
   * parameters that don't change.
   * @type {string}
   * @const
   * @private
   */
  SPEECH_SERVER_URL_: 'https://www.google.com/speech-api/v2/synthesize?' +
      'enc=mpeg&client=chromium',

  /**
   * A mapping from language and gender to voice name, hardcoded for now
   * until the speech synthesis server capabilities response provides this.
   * The key of this map is of the form '<lang>-<gender>'.
   * @type {Object<string>}
   * @private
   */
  LANG_AND_GENDER_TO_VOICE_NAME_: {
    'en-gb-male': 'rjs',
    'en-gb-female': 'fis',
  },

  /**
   * The arguments passed to the onSpeak event handler for the utterance
   * that's currently being spoken. Should be null when no object is
   * pending.
   *
   * @type {?{utterance: string, options: Object, callback: Function}}
   * @private
   */
  currentUtterance_: null,

  /**
   * A mapping from voice name to language and gender, derived from the
   * manifest file.  This is used in case the speech synthesis request
   * specifies a voice name but doesn't specify a language code or gender.
   * @type {Object<{lang: string, gender: string}>}
   * @private
   */
  voiceNameToLangAndGender_: {},

  /**
   * This is the main function called to initialize this extension.
   * Initializes data structures and adds event listeners.
   */
  init() {
    // Get voices from manifest.
    const voices = chrome.runtime.getManifest().tts_engine.voices;
    for (let i = 0; i < voices.length; i++) {
      this.voiceNameToLangAndGender_[voices[i].voice_name] = {
        lang: voices[i].lang,
        gender: voices[i].gender,
      };
    }

    chrome.offscreen.createDocument({
      url: chrome.runtime.getURL('audio.html'),
      reasons: ['AUDIO_PLAYBACK'],
      justification: 'Use the audio element',
    });

    // Install event listeners for the ttsEngine API.
    chrome.ttsEngine.onSpeak.addListener(this.onSpeak_.bind(this));
    chrome.ttsEngine.onStop.addListener(this.onStop_.bind(this));
    chrome.ttsEngine.onPause.addListener(this.onPause_.bind(this));
    chrome.ttsEngine.onResume.addListener(this.onResume_.bind(this));

    chrome.runtime.onMessage.addListener(message => {
      switch (message.command) {
        case 'onStart':
          if (this.currentUtterance_) {
            this.currentUtterance_.callback({'type': 'start', 'charIndex': 0});
          }
          break;
        case 'onStop':
          this.onStop_();
          break;
      }
    });
  },

  /**
   * Handler for the chrome.ttsEngine.onSpeak interface.
   * Gets Chrome's Google API key and then uses it to generate a request
   * url for the requested speech utterance. Sets that url as the source
   * of the HTML5 audio element.
   * @param {string} utterance The text to be spoken.
   * @param {Object} options Options to control the speech, as defined
   *     in the Chrome ttsEngine extension API.
   * @private
   */
  onSpeak_(utterance, options, callback) {
    // Ignore the utterance if it is empty. Continue such processing causes no
    // speech and fails all subsequent calls to process additional utterances.
    if (utterance.length === 0) {
      callback({'type': 'end', 'charIndex': 0});
      return;
    }

    // Truncate the utterance if it's too long. Both Chrome's tts
    // extension api and the web speech api specify 32k as the
    // maximum limit for an utterance.
    if (utterance.length > 32768) {
      utterance = utterance.substr(0, 32768);
    }

    try {
      // First, stop any pending audio.
      this.onStop_();

      this.currentUtterance_ = {
        utterance: utterance,
        options: options,
        callback: callback,
      };

      chrome.runtime.sendMessage({
        command: 'setCurrentUtterance',
        currentUtterance: this.currentUtterance_,
      });

      let lang = options.lang;
      let gender = options.gender;
      if (options.voiceName) {
        lang = this.voiceNameToLangAndGender_[options.voiceName].lang;
        gender = this.voiceNameToLangAndGender_[options.voiceName].gender;
      }

      if (!lang) {
        lang = navigator.language;
      }

      // Look up the specific voice name for this language and gender.
      // If it's not in the map, it doesn't matter - the language will
      // be used directly. This is only used for languages where more
      // than one gender is actually available.
      const key = lang.toLowerCase() + '-' + gender;
      const voiceName = this.LANG_AND_GENDER_TO_VOICE_NAME_[key];

      let url = this.SPEECH_SERVER_URL_;
      chrome.systemPrivate.getApiKey(key => {
        url += '&key=' + key;
        url += '&text=' + encodeURIComponent(utterance);
        url += '&lang=' + lang.toLowerCase();

        if (voiceName) {
          url += '&name=' + voiceName;
        }

        if (options.rate) {
          // Input rate is between 0.1 and 10.0 with a default of 1.0.
          // Output speed is between 0.0 and 1.0 with a default of 0.5.
          url += '&speed=' + (options.rate / 2.0);
        }

        if (options.pitch) {
          // Input pitch is between 0.0 and 2.0 with a default of 1.0.
          // Output pitch is between 0.0 and 1.0 with a default of 0.5.
          url += '&pitch=' + (options.pitch / 2.0);
        }

        // This begins loading the audio but does not play it.
        // When enough of the audio has loaded to begin playback,
        // the 'canplaythrough' handler will call this.onStart_,
        // which sends a start event to the ttsEngine callback and
        // then begins playing audio.
        chrome.runtime.sendMessage({command: 'setUrl', url});
      });
    } catch (err) {
      console.error(String(err));
      callback({'type': 'error', 'errorMessage': String(err)});
      this.currentUtterance_ = null;
    }
  },

  /**
   * Handler for the chrome.ttsEngine.onStop interface.
   * Called either when the ttsEngine API requests us to stop, or when
   * we reach the end of the audio stream. Pause the audio element to
   * silence it, and send a callback to the ttsEngine API to let it know
   * that we've completed. Note that the ttsEngine API manages callback
   * messages and will automatically replace the 'end' event with a
   * more specific callback like 'interrupted' when sending it to the
   * TTS client.
   * @private
   */
  onStop_() {
    if (this.currentUtterance_) {
      chrome.runtime.sendMessage({command: 'pause'});
      this.currentUtterance_.callback({
        'type': 'end',
        'charIndex': this.currentUtterance_.utterance.length,
      });
    }
    this.currentUtterance_ = null;
  },

  /**
   * Handler for the chrome.ttsEngine.onPause interface.
   * Pauses audio if we're in the middle of an utterance.
   * @private
   */
  onPause_() {
    if (this.currentUtterance_) {
      chrome.runtime.sendMessage({command: 'pause'});
    }
  },

  /**
   * Handler for the chrome.ttsEngine.onPause interface.
   * Resumes audio if we're in the middle of an utterance.
   * @private
   */
  onResume_() {
    if (this.currentUtterance_) {
      chrome.runtime.sendMessage({command: 'play'});
    }
  },

};

(new TtsExtension()).init();
