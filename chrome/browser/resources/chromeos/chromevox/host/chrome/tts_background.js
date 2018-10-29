// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 *
 */

goog.provide('cvox.TtsBackground');

goog.require('PanelCommand');
goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeTtsBase');
goog.require('cvox.ChromeVox');
goog.require('goog.i18n.MessageFormat');


/**
 * @constructor
 * @param {string} textString The string of text to be spoken.
 * @param {Object} properties Speech properties to use for this utterance.
 */
cvox.Utterance = function(textString, properties) {
  this.textString = textString;
  this.properties = properties;
  this.id = cvox.Utterance.nextUtteranceId_++;
};

/**
 * The next utterance id to use.
 * @type {number}
 * @private
 */
cvox.Utterance.nextUtteranceId_ = 1;

/**
 * @constructor
 * @extends {cvox.ChromeTtsBase}
 */
cvox.TtsBackground = function() {
  goog.base(this);

  this.lastEventType = 'end';

  /** @private {number} */
  this.currentPunctuationEcho_ =
      parseInt(localStorage[cvox.AbstractTts.PUNCTUATION_ECHO] || 1, 10);

  /**
   * @type {!Array<{name:(string),
   * msg:(string),
   * regexp:(RegExp),
   * clear:(boolean)}>}
   * @private
   */
  this.punctuationEchoes_ = [
    /**
     * Punctuation echoed for the 'none' option.
     */
    {
      name: 'none',
      msg: 'no_punctuation',
      regexp: /[-$#"()*;:<>\n\\\/+='~`@_]/g,
      clear: true
    },

    /**
     * Punctuation echoed for the 'some' option.
     */
    {
      name: 'some',
      msg: 'some_punctuation',
      regexp: /[$#"*<>\\\/\{\}+=~`%\u2022]/g,
      clear: false
    },

    /**
     * Punctuation echoed for the 'all' option.
     */
    {
      name: 'all',
      msg: 'all_punctuation',
      regexp: /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%\u2022]/g,
      clear: false
    }
  ];

  /**
   * A list of punctuation characters that should always be spliced into output
   * even with literal word substitutions.
   * This is important for tts prosity.
   * @type {!Array<string>}
   * @private
   */
  this.retainPunctuation_ = [';', '?', '!', '\''];

  /**
   * The id of a callback returned from setTimeout.
   * @type {number|undefined}
   */
  this.timeoutId_;

  try {
    /**
     * @type {Object<string>}
     * @private
     * @const
     */
    this.PHONETIC_MAP_ =
        /** @type {Object<string>} */ (JSON.parse(Msgs.getMsg('phonetic_map')));
  } catch (e) {
    console.log('Error; unable to parse phonetic map msg.');
  }

  /**
   * Capturing tts event listeners.
   * @type {Array<cvox.TtsCapturingEventListener>}
   * @private
   */
  this.capturingTtsEventListeners_ = [];

  /**
   * The current utterance.
   * @type {cvox.Utterance}
   * @private
   */
  this.currentUtterance_ = null;

  /**
   * The utterance queue.
   * @type {Array<cvox.Utterance>}
   * @private
   */
  this.utteranceQueue_ = [];

  /**
   * The current voice name.
   * @type {string}
   */
  this.currentVoice;

  // TODO(dtseng): Done while migrating away from using localStorage.
  if (localStorage['voiceName']) {
    chrome.storage.local.set({voiceName: localStorage['voiceName']});
    delete localStorage['voiceName'];
  }

  if (window.speechSynthesis) {
    window.speechSynthesis.onvoiceschanged = function() {
      chrome.storage.local.get({voiceName: ''}, function(items) {
        this.updateVoice_(items.voiceName);
      }.bind(this));
    }.bind(this);
  } else {
    // SpeechSynthesis API is not available on chromecast. Call
    // updateVoice_ to set the one and only voice as the current
    // voice.
    this.updateVoice_('');
  }

  chrome.storage.onChanged.addListener(function(changes, namespace) {
    if (changes.voiceName) {
      this.updateVoice_(changes.voiceName.newValue);
    }
  }.bind(this));

  // Migration: localStorage tts properties -> Chrome pref settings.
  if (localStorage['rate']) {
    chrome.settingsPrivate.setPref(
        'settings.tts.speech_rate', parseFloat(localStorage['rate']));
    delete localStorage['rate'];
  }
  if (localStorage['pitch']) {
    chrome.settingsPrivate.setPref(
        'settings.tts.speech_pitch', parseFloat(localStorage['pitch']));
    delete localStorage['pitch'];
  }
  if (localStorage['volume']) {
    chrome.settingsPrivate.setPref(
        'settings.tts.speech_volume', parseFloat(localStorage['volume']));
    delete localStorage['volume'];
  }

  // At startup.
  chrome.settingsPrivate.getAllPrefs(this.updateFromPrefs_.bind(this, false));

  // At runtime.
  chrome.settingsPrivate.onPrefsChanged.addListener(
      this.updateFromPrefs_.bind(this, true));
};
goog.inherits(cvox.TtsBackground, cvox.ChromeTtsBase);


/**
 * The amount of time to wait before speaking a phonetic word for a
 * letter.
 * @type {number}
 * @private
 * @const
 */
cvox.TtsBackground.PHONETIC_DELAY_MS_ = 1000;

/**
 * The list of properties allowed to be passed to the chrome.tts.speak API.
 * Anything outside this list will be stripped.
 * @type {Array<string>}
 * @private
 * @const
 */
cvox.TtsBackground.ALLOWED_PROPERTIES_ = [
  'desiredEventTypes', 'enqueue', 'extensionId', 'gender', 'lang', 'onEvent',
  'pitch', 'rate', 'requiredEventTypes', 'voiceName', 'volume'
];


/** @override */
cvox.TtsBackground.prototype.speak = function(
    textString, queueMode, properties) {
  goog.base(this, 'speak', textString, queueMode, properties);

  if (this.ttsProperties[cvox.AbstractTts.VOLUME] === 0) {
    return this;
  }

  if (!properties) {
    properties = {};
  }

  // Chunk to improve responsiveness. Use a replace/split pattern in order to
  // retain the original punctuation.
  var splitTextString = textString.replace(/([-\n\r.,!?;])(\s)/g, '$1$2|');
  splitTextString = splitTextString.split('|');
  // Since we are substituting the chunk delimiters back into the string, only
  // recurse when there are more than 2 split items. This should result in only
  // one recursive call.
  if (splitTextString.length > 2) {
    var startCallback = properties['startCallback'];
    var endCallback = properties['endCallback'];
    var onEvent = properties['onEvent'];
    for (var i = 0; i < splitTextString.length; i++) {
      var propertiesCopy = {};
      for (var p in properties) {
        propertiesCopy[p] = properties[p];
      }
      propertiesCopy['startCallback'] = i == 0 ? startCallback : null;
      propertiesCopy['endCallback'] =
          i == (splitTextString.length - 1) ? endCallback : null;
      propertiesCopy['onEvent'] =
          i == (splitTextString.length - 1) ? onEvent : null;
      this.speak(splitTextString[i], queueMode, propertiesCopy);
      queueMode = cvox.QueueMode.QUEUE;
    }
    return this;
  }

  textString = this.preprocess(textString, properties);

  // TODO(dtseng): Google TTS has bad performance when speaking numbers. This
  // pattern causes ChromeVox to read numbers as digits rather than words.
  textString = this.getNumberAsDigits_(textString);

  // TODO(dtseng): some TTS engines don't handle strings that don't produce any
  // speech very well. Handle empty and whitespace only strings (including
  // non-breaking space) here to mitigate the issue somewhat.
  if (cvox.TtsBackground.SKIP_WHITESPACE_.test(textString)) {
    // Explicitly call start and end callbacks before skipping this text.
    if (properties['startCallback']) {
      try {
        properties['startCallback']();
      } catch (e) {
      }
    }
    if (properties['endCallback']) {
      try {
        properties['endCallback']();
      } catch (e) {
      }
    }
    if (queueMode === cvox.QueueMode.FLUSH) {
      this.stop();
    }
    return this;
  }

  var mergedProperties = this.mergeProperties(properties);

  if (this.currentVoice && this.currentVoice !== constants.SYSTEM_VOICE) {
    mergedProperties['voiceName'] = this.currentVoice;
  }

  if (queueMode == cvox.QueueMode.CATEGORY_FLUSH &&
      !mergedProperties['category']) {
    queueMode = cvox.QueueMode.FLUSH;
  }

  var utterance = new cvox.Utterance(textString, mergedProperties);
  this.speakUsingQueue_(utterance, queueMode);
  return this;
};

/**
 * Use the speech queue to handle the given speech request.
 * @param {cvox.Utterance} utterance The utterance to speak.
 * @param {cvox.QueueMode} queueMode The queue mode.
 * @private
 */
cvox.TtsBackground.prototype.speakUsingQueue_ = function(utterance, queueMode) {
  // First, take care of removing the current utterance and flushing
  // anything from the queue we need to. If we remove the current utterance,
  // make a note that we're going to stop speech.
  if (queueMode == cvox.QueueMode.FLUSH ||
      queueMode == cvox.QueueMode.CATEGORY_FLUSH) {
    (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();

    if (this.shouldCancel_(this.currentUtterance_, utterance, queueMode)) {
      this.cancelUtterance_(this.currentUtterance_);
      this.currentUtterance_ = null;
    }
    var i = 0;
    while (i < this.utteranceQueue_.length) {
      if (this.shouldCancel_(this.utteranceQueue_[i], utterance, queueMode)) {
        this.cancelUtterance_(this.utteranceQueue_[i]);
        this.utteranceQueue_.splice(i, 1);
      } else {
        i++;
      }
    }
  }

  // Next, add the new utterance to the queue.
  this.utteranceQueue_.push(utterance);

  // Now start speaking the next item in the queue.
  this.startSpeakingNextItemInQueue_();
};

/**
 * If nothing is speaking, pop the first item off the speech queue and
 * start speaking it. This is called when a speech request is made and
 * when the current utterance finishes speaking.
 * @private
 */
cvox.TtsBackground.prototype.startSpeakingNextItemInQueue_ = function() {
  if (this.currentUtterance_) {
    return;
  }

  if (this.utteranceQueue_.length == 0) {
    return;
  }

  // There is no voice to speak with (e.g. the tts system has not fully
  // initialized).
  if (!this.currentVoice) {
    return;
  }

  this.currentUtterance_ = this.utteranceQueue_.shift();
  var utterance = this.currentUtterance_;
  var utteranceId = utterance.id;

  utterance.properties['onEvent'] = goog.bind(function(event) {
    this.onTtsEvent_(event, utteranceId);
  }, this);

  var validatedProperties = {};
  for (var i = 0; i < cvox.TtsBackground.ALLOWED_PROPERTIES_.length; i++) {
    var p = cvox.TtsBackground.ALLOWED_PROPERTIES_[i];
    if (utterance.properties[p]) {
      validatedProperties[p] = utterance.properties[p];
    }
  }

  // Update the caption panel.
  if (utterance.properties && utterance.properties['pitch'] &&
      utterance.properties['pitch'] < this.ttsProperties['pitch']) {
    (new PanelCommand(
         PanelCommandType.ADD_ANNOTATION_SPEECH, utterance.textString))
        .send();
  } else {
    (new PanelCommand(PanelCommandType.ADD_NORMAL_SPEECH, utterance.textString))
        .send();
  }

  chrome.tts.speak(utterance.textString, validatedProperties);
};

/**
 * Called when we get a speech event from Chrome. We ignore any event
 * that doesn't pertain to the current utterance, but when speech starts
 * or ends we optionally call callback functions, and start speaking the
 * next utterance if there's another one enqueued.
 * @param {Object} event The TTS event from chrome.
 * @param {number} utteranceId The id of the associated utterance.
 * @private
 */
cvox.TtsBackground.prototype.onTtsEvent_ = function(event, utteranceId) {
  this.lastEventType = event['type'];

  // Ignore events sent on utterances other than the current one.
  if (!this.currentUtterance_ || utteranceId != this.currentUtterance_.id) {
    return;
  }

  var utterance = this.currentUtterance_;

  switch (event.type) {
    case 'start':
      this.capturingTtsEventListeners_.forEach(function(listener) {
        listener.onTtsStart();
      });
      if (utterance.properties['startCallback']) {
        try {
          utterance.properties['startCallback']();
        } catch (e) {
        }
      }
      break;
    case 'end':
      // End callbacks could cause additional speech to queue up.
      this.currentUtterance_ = null;
      this.capturingTtsEventListeners_.forEach(function(listener) {
        listener.onTtsEnd();
      });
      if (utterance.properties['endCallback']) {
        try {
          utterance.properties['endCallback']();
        } catch (e) {
        }
      }
      this.startSpeakingNextItemInQueue_();
      break;
    case 'interrupted':
      this.cancelUtterance_(utterance);
      this.currentUtterance_ = null;
      for (var i = 0; i < this.utteranceQueue_.length; i++) {
        this.cancelUtterance_(this.utteranceQueue_[i]);
      }
      this.utteranceQueue_.length = 0;
      this.capturingTtsEventListeners_.forEach(function(listener) {
        listener.onTtsInterrupted();
      });
      break;
    case 'error':
      this.onError_(event['errorMessage']);
      this.startSpeakingNextItemInQueue_();
      break;
  }
};

/**
 * Determines if |utteranceToCancel| should be canceled (interrupted if
 * currently speaking, or removed from the queue if not), given the new
 * utterance we want to speak and the queue mode. If the queue mode is
 * QUEUE or FLUSH, the logic is straightforward. If the queue mode is
 * CATEGORY_FLUSH, we only flush utterances with the same category.
 *
 * @param {cvox.Utterance} utteranceToCancel The utterance in question.
 * @param {cvox.Utterance} newUtterance The new utterance we're enqueueing.
 * @param {cvox.QueueMode} queueMode The queue mode.
 * @return {boolean} True if this utterance should be canceled.
 * @private
 */
cvox.TtsBackground.prototype.shouldCancel_ = function(
    utteranceToCancel, newUtterance, queueMode) {
  if (!utteranceToCancel) {
    return false;
  }
  if (utteranceToCancel.properties['doNotInterrupt']) {
    return false;
  }
  switch (queueMode) {
    case cvox.QueueMode.QUEUE:
      return false;
    case cvox.QueueMode.FLUSH:
      return true;
    case cvox.QueueMode.CATEGORY_FLUSH:
      return (
          utteranceToCancel.properties['category'] ==
          newUtterance.properties['category']);
  }
  return false;
};

/**
 * Do any cleanup necessary to cancel an utterance, like callings its
 * callback function if any.
 * @param {cvox.Utterance} utterance The utterance to cancel.
 * @private
 */
cvox.TtsBackground.prototype.cancelUtterance_ = function(utterance) {
  if (utterance && utterance.properties['endCallback']) {
    try {
      utterance.properties['endCallback'](true);
    } catch (e) {
    }
  }
};

/** @override */
cvox.TtsBackground.prototype.increaseOrDecreaseProperty = function(
    propertyName, increase) {
  goog.base(this, 'increaseOrDecreaseProperty', propertyName, increase);

  var pref;
  switch (propertyName) {
    case cvox.AbstractTts.RATE:
      pref = 'settings.tts.speech_rate';
      break;
    case cvox.AbstractTts.PITCH:
      pref = 'settings.tts.speech_pitch';
      break;
    case cvox.AbstractTts.VOLUME:
      pref = 'settings.tts.speech_volume';
      break;
    default:
      return;
  }
  var value = this.ttsProperties[propertyName];
  chrome.settingsPrivate.setPref(pref, value);
};

/** @override */
cvox.TtsBackground.prototype.isSpeaking = function() {
  goog.base(this, 'isSpeaking');
  return this.lastEventType != 'end';
};

/** @override */
cvox.TtsBackground.prototype.stop = function() {
  goog.base(this, 'stop');

  this.cancelUtterance_(this.currentUtterance_);
  this.currentUtterance_ = null;

  for (var i = 0; i < this.utteranceQueue_.length; i++) {
    this.cancelUtterance_(this.utteranceQueue_[i]);
  }

  this.utteranceQueue_.length = 0;

  (new PanelCommand(PanelCommandType.CLEAR_SPEECH)).send();
  chrome.tts.stop();

  this.capturingTtsEventListeners_.forEach(function(listener) {
    listener.onTtsInterrupted();
  });
};

/** @override */
cvox.TtsBackground.prototype.addCapturingEventListener = function(listener) {
  this.capturingTtsEventListeners_.push(listener);
};

/**
 * An error handler passed as a callback to chrome.tts.speak.
 * @param {string} errorMessage Describes the error (set by onEvent).
 * @private
 */
cvox.TtsBackground.prototype.onError_ = function(errorMessage) {
  this.updateVoice_(this.currentVoice);
};

/**
 * @override
 */
cvox.TtsBackground.prototype.preprocess = function(text, properties) {
  properties = properties ? properties : {};

  // Perform generic processing.
  text = goog.base(this, 'preprocess', text, properties);

  // Perform any remaining processing such as punctuation expansion.
  var pE = null;
  if (properties[cvox.AbstractTts.PUNCTUATION_ECHO]) {
    for (var i = 0; pE = this.punctuationEchoes_[i]; i++) {
      if (properties[cvox.AbstractTts.PUNCTUATION_ECHO] == pE.name) {
        break;
      }
    }
  } else {
    pE = this.punctuationEchoes_[this.currentPunctuationEcho_];
  }
  text = text.replace(pE.regexp, this.createPunctuationReplace_(pE.clear));

  // Try pronouncing phonetically for single characters. Cancel previous calls
  // to pronouncePhonetically_ if we fail to pronounce on this invokation or if
  // this text is math which should never be pronounced phonetically.
  if (properties.math || !properties['phoneticCharacters'] ||
      !this.pronouncePhonetically_(text)) {
    this.clearTimeout_();
  }

  //  Remove all whitespace from the beginning and end, and collapse all
  // inner strings of whitespace to a single space.
  text = text.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');

  return text;
};


/** @override */
cvox.TtsBackground.prototype.toggleSpeechOnOrOff = function() {
  var previousValue = this.ttsProperties[cvox.AbstractTts.VOLUME];
  var toggle = function() {
    if (previousValue == 0) {
      this.ttsProperties[cvox.AbstractTts.VOLUME] = 1;
    } else {
      this.ttsProperties[cvox.AbstractTts.VOLUME] = 0;
    }
  }.bind(this);

  if (previousValue == 0) {
    toggle();
  } else {
    // Let the caller make any last minute announcements in the current call
    // stack.
    setTimeout(toggle, 0);
  }

  return previousValue == 0;
};


/**
 * Method that cycles among the available punctuation echo levels.
 * @return {string} The resulting punctuation level message id.
 */
cvox.TtsBackground.prototype.cyclePunctuationEcho = function() {
  this.currentPunctuationEcho_ =
      (this.currentPunctuationEcho_ + 1) % this.punctuationEchoes_.length;
  localStorage[cvox.AbstractTts.PUNCTUATION_ECHO] =
      this.currentPunctuationEcho_;
  return this.punctuationEchoes_[this.currentPunctuationEcho_].msg;
};


/**
 * Converts a number into space-separated digits.
 * For numbers containing 4 or fewer digits, we return the original number.
 * This ensures that numbers like 123,456 or 2011 are not "digitized" while
 * 123456 is.
 * @param {string} text The text to process.
 * @return {string} A string with all numbers converted.
 * @private
 */
cvox.TtsBackground.prototype.getNumberAsDigits_ = function(text) {
  return text.replace(/\d+/g, function(num) {
    if (num.length <= 4) {
      return num;
    }
    return num.split('').join(' ');
  });
};


/**
 * Constructs a function for string.replace that handles description of a
 *  punctuation character.
 * @param {boolean} clear Whether we want to use whitespace in place of match.
 * @return {function(string): string} The replacement function.
 * @private
 */
cvox.TtsBackground.prototype.createPunctuationReplace_ = function(clear) {
  return goog.bind(function(match) {
    var retain = this.retainPunctuation_.indexOf(match) != -1 ? match : ' ';
    return clear ? retain :
                   ' ' +
            (new goog.i18n.MessageFormat(
                 Msgs.getMsg(cvox.AbstractTts.CHARACTER_DICTIONARY[match])))
                .format({'COUNT': 1}) +
            retain + ' ';
  }, this);
};


/**
 * Pronounces single letters phonetically after some timeout.
 * @param {string} text The text.
 * @return {boolean} True if the text resulted in speech.
 * @private
 */
cvox.TtsBackground.prototype.pronouncePhonetically_ = function(text) {
  if (!this.PHONETIC_MAP_) {
    return false;
  }
  text = text.toLowerCase();
  text = this.PHONETIC_MAP_[text];
  if (text) {
    this.clearTimeout_();
    var self = this;
    this.timeoutId_ = setTimeout(function() {
      self.speak(text, cvox.QueueMode.QUEUE);
    }, cvox.TtsBackground.PHONETIC_DELAY_MS_);
    return true;
  }
  return false;
};


/**
 * Clears the last timeout set via setTimeout.
 * @private
 */
cvox.TtsBackground.prototype.clearTimeout_ = function() {
  if (goog.isDef(this.timeoutId_)) {
    clearTimeout(this.timeoutId_);
    this.timeoutId_ = undefined;
  }
};


/**
 * Update the current voice used to speak based upon values in storage. If that
 * does not succeed, fallback to use system locale when picking a voice.
 * @param {string} voiceName Voice name to set.
 * @param {function(string) : void=} opt_callback Called when the voice is
 * determined.
 * @private
 */
cvox.TtsBackground.prototype.updateVoice_ = function(voiceName, opt_callback) {
  chrome.tts.getVoices(goog.bind(function(voices) {
    let systemVoice = {voiceName: constants.SYSTEM_VOICE};
    voices.unshift(systemVoice);
    let newVoice = voices.find((v) => {
      return v.voiceName == voiceName;
    }) ||
        systemVoice;
    if (newVoice) {
      this.currentVoice = newVoice.voiceName;
      this.startSpeakingNextItemInQueue_();
    }
    if (opt_callback)
      opt_callback(this.currentVoice);
  }, this));
};

/**
 * @param {boolean} announce
 * @param {Array<chrome.settingsPrivate.PrefObject>} prefs
 * @private
 */
cvox.TtsBackground.prototype.updateFromPrefs_ = function(announce, prefs) {
  prefs.forEach((pref) => {
    var msg;
    var propertyName;
    switch (pref.key) {
      case 'settings.tts.speech_rate':
        propertyName = cvox.AbstractTts.RATE;
        msg = 'announce_rate';
        break;
      case 'settings.tts.speech_pitch':
        propertyName = cvox.AbstractTts.PITCH;
        msg = 'announce_pitch';
        break;
      case 'settings.tts.speech_volume':
        propertyName = cvox.AbstractTts.VOLUME;
        msg = 'announce_volume';
        break;
      default:
        return;
    }

    this.ttsProperties[propertyName] = pref.value;

    if (!announce)
      return;

    var valueAsPercent =
        Math.round(this.propertyToPercentage(propertyName) * 100);
    var announcement = Msgs.getMsg(msg, [valueAsPercent]);
    cvox.ChromeVox.tts.speak(
        announcement, cvox.QueueMode.FLUSH,
        cvox.AbstractTts.PERSONALITY_ANNOTATION);
  });
};

/** @private {RegExp} */
cvox.TtsBackground.SKIP_WHITESPACE_ = /^[\s\u00a0]*$/;
