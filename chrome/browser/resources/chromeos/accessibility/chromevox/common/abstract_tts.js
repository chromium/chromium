// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for Text-to-Speech engines that actually transform
 * text to speech.
 *
 */

goog.provide('AbstractTts');

goog.require('Msgs');
goog.require('TtsInterface');
goog.require('goog.i18n.MessageFormat');

/**
 * Creates a new instance.
 * @implements {TtsInterface}
 */
AbstractTts = class {
  constructor() {
    this.ttsProperties = new Object();

    /**
     * Default value for TTS properties.
     * Note that these as well as the subsequent properties might be different
     * on different host platforms (like Chrome, Android, etc.).
     * @type {{pitch : number,
     *         rate: number,
     *         volume: number}}
     * @protected
     */
    this.propertyDefault = {'rate': 0.5, 'pitch': 0.5, 'volume': 0.5};

    /**
     * Min value for TTS properties.
     * @type {{pitch : number,
     *         rate: number,
     *         volume: number}}
     * @protected
     */
    this.propertyMin = {'rate': 0.0, 'pitch': 0.0, 'volume': 0.0};

    /**
     * Max value for TTS properties.
     * @type {{pitch : number,
     *         rate: number,
     *         volume: number}}
     * @protected
     */
    this.propertyMax = {'rate': 1.0, 'pitch': 1.0, 'volume': 1.0};

    /**
     * Step value for TTS properties.
     * @type {{pitch : number,
     *         rate: number,
     *         volume: number}}
     * @protected
     */
    this.propertyStep = {'rate': 0.1, 'pitch': 0.1, 'volume': 0.1};

    if (AbstractTts.substitutionDictionaryRegexp_ == undefined) {
      // Create an expression that matches all words in the substitution
      // dictionary.
      const symbols = [];
      for (const symbol in AbstractTts.SUBSTITUTION_DICTIONARY) {
        symbols.push(symbol);
      }
      const expr = '(' + symbols.join('|') + ')';
      AbstractTts.substitutionDictionaryRegexp_ = new RegExp(expr, 'ig');
    }
  }

  /** @override */
  speak(textString, queueMode, properties) {
    return this;
  }

  /** @override */
  isSpeaking() {
    return false;
  }

  /** @override */
  stop() {}

  /** @override */
  addCapturingEventListener(listener) {}

  /** @override */
  removeCapturingEventListener(listener) {}

  /** @override */
  increaseOrDecreaseProperty(propertyName, increase) {
    const min = this.propertyMin[propertyName];
    const max = this.propertyMax[propertyName];
    const step = this.propertyStep[propertyName];
    let current = this.ttsProperties[propertyName];
    current = increase ? current + step : current - step;
    this.ttsProperties[propertyName] = Math.max(Math.min(current, max), min);
  }

  /**
   * Converts an engine property value to a percentage from 0.00 to 1.00.
   * @param {string} property The property to convert.
   * @return {?number} The percentage of the property.
   */
  propertyToPercentage(property) {
    return (this.ttsProperties[property] - this.propertyMin[property]) /
        Math.abs(this.propertyMax[property] - this.propertyMin[property]);
  }

  /**
   * Merges the given properties with the default ones. Always returns a
   * new object, so that you can safely modify the result of mergeProperties
   * without worrying that you're modifying an object used elsewhere.
   * @param {Object=} properties The properties to merge with the current ones.
   * @return {Object} The merged properties.
   * @protected
   */
  mergeProperties(properties) {
    const mergedProperties = new Object();
    let p;
    if (this.ttsProperties) {
      for (p in this.ttsProperties) {
        mergedProperties[p] = this.ttsProperties[p];
      }
    }
    if (properties) {
      const tts = AbstractTts;
      if (typeof (properties[tts.VOLUME]) == 'number') {
        mergedProperties[tts.VOLUME] = properties[tts.VOLUME];
      }
      if (typeof (properties[tts.PITCH]) == 'number') {
        mergedProperties[tts.PITCH] = properties[tts.PITCH];
      }
      if (typeof (properties[tts.RATE]) == 'number') {
        mergedProperties[tts.RATE] = properties[tts.RATE];
      }
      if (typeof (properties[tts.LANG]) == 'string') {
        mergedProperties[tts.LANG] = properties[tts.LANG];
      }

      const context = this;
      const mergeRelativeProperty = function(abs, rel) {
        if (typeof (properties[rel]) == 'number' &&
            typeof (mergedProperties[abs]) == 'number') {
          mergedProperties[abs] += properties[rel];
          const min = context.propertyMin[abs];
          const max = context.propertyMax[abs];
          if (mergedProperties[abs] > max) {
            mergedProperties[abs] = max;
          } else if (mergedProperties[abs] < min) {
            mergedProperties[abs] = min;
          }
        }
      };

      mergeRelativeProperty(tts.VOLUME, tts.RELATIVE_VOLUME);
      mergeRelativeProperty(tts.PITCH, tts.RELATIVE_PITCH);
      mergeRelativeProperty(tts.RATE, tts.RELATIVE_RATE);
    }

    for (p in properties) {
      if (!mergedProperties.hasOwnProperty(p)) {
        mergedProperties[p] = properties[p];
      }
    }

    return mergedProperties;
  }

  /**
   * Method to preprocess text to be spoken properly by a speech
   * engine.
   *
   * 1. Replace any single character with a description of that character.
   *
   * 2. Convert all-caps words to lowercase if they don't look like an
   *    acronym / abbreviation.
   *
   * @param {string} text A text string to be spoken.
   * @param {Object= } properties Out parameter populated with how to speak the
   *     string.
   * @return {string} The text formatted in a way that will sound better by
   *     most speech engines.
   * @protected
   */
  preprocess(text, properties) {
    if (text.length == 1 && text >= 'A' && text <= 'Z') {
      // Describe capital letters according to user's setting.
      if (localStorage['capitalStrategy'] == 'increasePitch') {
        for (const prop in AbstractTts.PERSONALITY_CAPITAL) {
          if (properties[prop] === undefined) {
            properties[prop] = AbstractTts.PERSONALITY_CAPITAL[prop];
          }
        }
      } else if (localStorage['capitalStrategy'] == 'announceCapitals') {
        text = Msgs.getMsg('announce_capital_letter', [text]);
      }
    }

    if (localStorage['usePitchChanges'] === 'false') {
      delete properties.relativePitch;
    }

    // Substitute all symbols in the substitution dictionary. This is pretty
    // efficient because we use a single regexp that matches all symbols
    // simultaneously.
    text = text.replace(
        AbstractTts.substitutionDictionaryRegexp_, function(symbol) {
          return ' ' + AbstractTts.SUBSTITUTION_DICTIONARY[symbol] + ' ';
        });

    // Handle single characters that we want to make sure we pronounce.
    if (text.length == 1) {
      return AbstractTts.CHARACTER_DICTIONARY[text] ?
          (new goog.i18n.MessageFormat(
               Msgs.getMsg(AbstractTts.CHARACTER_DICTIONARY[text])))
              .format({'COUNT': 1}) :
          text.toUpperCase();
    }

    // Expand all repeated characters.
    text = text.replace(
        AbstractTts.repetitionRegexp_, AbstractTts.repetitionReplace_);

    return text;
  }

  /**
   * Constructs a description of a repeated character. Use as a param to
   * string.replace.
   * @param {string} match The matching string.
   * @return {string} The description.
   * @private
   */
  static repetitionReplace_(match) {
    const count = match.length;
    return ' ' +
        (new goog.i18n.MessageFormat(
             Msgs.getMsg(AbstractTts.CHARACTER_DICTIONARY[match[0]])))
            .format({'COUNT': count}) +
        ' ';
  }

  /**
   * @override
   */
  getDefaultProperty(property) {
    return this.propertyDefault[property];
  }

  /** @override */
  toggleSpeechOnOrOff() {
    return true;
  }

  /** @override */
  resetTextToSpeechSettings() {
    for (const [key, value] of Object.entries(this.propertyDefault)) {
      this.ttsProperties[key] = value;
    }
  }
};


/**
 * Default TTS properties for this TTS engine.
 * @type {Object}
 * @protected
 */
AbstractTts.prototype.ttsProperties;


/** TTS rate property. @type {string} */
AbstractTts.RATE = 'rate';
/** TTS pitch property. @type {string} */
AbstractTts.PITCH = 'pitch';
/** TTS volume property. @type {string} */
AbstractTts.VOLUME = 'volume';
/** TTS language property. @type {string} */
AbstractTts.LANG = 'lang';

/** TTS relative rate property. @type {string} */
AbstractTts.RELATIVE_RATE = 'relativeRate';
/** TTS relative pitch property. @type {string} */
AbstractTts.RELATIVE_PITCH = 'relativePitch';
/** TTS relative volume property. @type {string} */
AbstractTts.RELATIVE_VOLUME = 'relativeVolume';

/** TTS color property (for the lens display). @type {string} */
AbstractTts.COLOR = 'color';
/** TTS CSS font-weight property (for the lens display). @type {string} */
AbstractTts.FONT_WEIGHT = 'fontWeight';

/** TTS punctuation-echo property. @type {string} */
AbstractTts.PUNCTUATION_ECHO = 'punctuationEcho';

/**
 * List of punctuation echoes that the user can cycle through.
 * @type {!Array<{name:(string),
 * msg:(string),
 * regexp:(RegExp),
 * clear:(boolean)}>}
 */
AbstractTts.PUNCTUATION_ECHOES = [
  // Punctuation echoed for the 'none' option.
  {
    name: 'none',
    msg: 'no_punctuation',
    regexp: /[-$#"()*;:<>\n\\\/+='~`@_]/g,
    clear: true
  },

  // Punctuation echoed for the 'some' option.
  {
    name: 'some',
    msg: 'some_punctuation',
    regexp: /[$#"*<>\\\/\{\}+=~`%\u2022]/g,
    clear: false
  },

  // Punctuation echoed for the 'all' option.
  {
    name: 'all',
    msg: 'all_punctuation',
    regexp: /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%\u2022]/g,
    clear: false
  }
];

/** TTS pause property. @type {string} */
AbstractTts.PAUSE = 'pause';

/**
 * TTS personality for annotations - text spoken by ChromeVox that
 * elaborates on a user interface element but isn't displayed on-screen.
 * @type {!Object}
 */
AbstractTts.PERSONALITY_ANNOTATION = {
  'relativePitch': -0.25,
  // TODO:(rshearer) Added this color change for I/O presentation.
  'color': 'yellow',
  'punctuationEcho': 'none'
};


/**
 * TTS personality for announcements - text spoken by ChromeVox that
 * isn't tied to any user interface elements.
 * @type {Object}
 */
AbstractTts.PERSONALITY_ANNOUNCEMENT = {
  'punctuationEcho': 'none'
};

/**
 * TTS personality for alerts from the system, such as battery level
 * warnings.
 * @type {Object}
 */
AbstractTts.PERSONALITY_SYSTEM_ALERT = {
  'punctuationEcho': 'none',
  'doNotInterrupt': true
};

/**
 * TTS personality for an aside - text in parentheses.
 * @type {Object}
 */
AbstractTts.PERSONALITY_ASIDE = {
  'relativePitch': -0.1,
  'color': '#669'
};


/**
 * TTS personality for capital letters.
 * @type {Object}
 */
AbstractTts.PERSONALITY_CAPITAL = {
  'relativePitch': 0.2
};


/**
 * TTS personality for deleted text.
 * @type {Object}
 */
AbstractTts.PERSONALITY_DELETED = {
  'punctuationEcho': 'none',
  'relativePitch': -0.6
};


/**
 * TTS personality for quoted text.
 * @type {Object}
 */
AbstractTts.PERSONALITY_QUOTE = {
  'relativePitch': 0.1,
  'color': '#b6b',
  'fontWeight': 'bold'
};


/**
 * TTS personality for strong or bold text.
 * @type {Object}
 */
AbstractTts.PERSONALITY_STRONG = {
  'relativePitch': 0.1,
  'color': '#b66',
  'fontWeight': 'bold'
};


/**
 * TTS personality for emphasis or italicized text.
 * @type {Object}
 */
AbstractTts.PERSONALITY_EMPHASIS = {
  'relativeVolume': 0.1,
  'relativeRate': -0.1,
  'color': '#6bb',
  'fontWeight': 'bold'
};


/**
 * Flag indicating if the TTS is being debugged.
 * @type {boolean}
 */
AbstractTts.DEBUG = true;


/**
 * Character dictionary. These symbols are replaced with their human readable
 * equivalents. This replacement only occurs for single character utterances.
 * @type {Object<string>}
 */
AbstractTts.CHARACTER_DICTIONARY = {
  ' ': 'space',
  '`': 'backtick',
  '~': 'tilde',
  '!': 'exclamation',
  '@': 'at',
  '#': 'pound',
  '$': 'dollar',
  '%': 'percent',
  '^': 'caret',
  '&': 'ampersand',
  '*': 'asterisk',
  '(': 'open_paren',
  ')': 'close_paren',
  '-': 'dash',
  '_': 'underscore',
  '=': 'equals',
  '+': 'plus',
  '[': 'left_bracket',
  ']': 'right_bracket',
  '{': 'left_brace',
  '}': 'right_brace',
  '|': 'pipe',
  ';': 'semicolon',
  ':': 'colon',
  ',': 'comma',
  '.': 'dot',
  '<': 'less_than',
  '>': 'greater_than',
  '/': 'slash',
  '?': 'question_mark',
  '"': 'quote',
  '\'': 'apostrophe',
  '\t': 'tab',
  '\r': 'return',
  '\n': 'new_line',
  '\\': 'backslash',
  '\u2022': 'bullet'
};


/**
 * Pronunciation dictionary regexp.
 * @type {RegExp};
 * @private
 */
AbstractTts.pronunciationDictionaryRegexp_;


/**
 * Substitution dictionary. These symbols or patterns are ALWAYS substituted
 * whenever they occur, so this should be reserved only for unicode characters
 * and characters that never have any different meaning in context.
 *
 * For example, do not include '$' here because $2 should be read as
 * "two dollars".
 * @type {Object<string>}
 */
AbstractTts.SUBSTITUTION_DICTIONARY = {
  '://': 'colon slash slash',
  '\u00bc': 'one fourth',
  '\u00bd': 'one half',
  '\u2190': 'left arrow',
  '\u2191': 'up arrow',
  '\u2192': 'right arrow',
  '\u2193': 'down arrow',
  '\u21d0': 'left double arrow',
  '\u21d1': 'up double arrow',
  '\u21d2': 'right double  arrow',
  '\u21d3': 'down double arrow',
  '\u21e6': 'left arrow',
  '\u21e7': 'up arrow',
  '\u21e8': 'right arrow',
  '\u21e9': 'down arrow',
  '\u2303': 'control',
  '\u2318': 'command',
  '\u2325': 'option',
  '\u25b2': 'up triangle',
  '\u25b3': 'up triangle',
  '\u25b4': 'up triangle',
  '\u25b5': 'up triangle',
  '\u25b6': 'right triangle',
  '\u25b7': 'right triangle',
  '\u25b8': 'right triangle',
  '\u25b9': 'right triangle',
  '\u25ba': 'right pointer',
  '\u25bb': 'right pointer',
  '\u25bc': 'down triangle',
  '\u25bd': 'down triangle',
  '\u25be': 'down triangle',
  '\u25bf': 'down triangle',
  '\u25c0': 'left triangle',
  '\u25c1': 'left triangle',
  '\u25c2': 'left triangle',
  '\u25c3': 'left triangle',
  '\u25c4': 'left pointer',
  '\u25c5': 'left pointer',
  '\uf8ff': 'apple'
};


/**
 * Substitution dictionary regexp.
 * @type {RegExp};
 * @private
 */
AbstractTts.substitutionDictionaryRegexp_;


/**
 * repetition filter regexp.
 * @type {RegExp}
 * @private
 */
AbstractTts.repetitionRegexp_ =
    /([-\/\\|!@#$%^&*\(\)=_+\[\]\{\}.?;'":<>\u2022])\1{2,}/g;

/** TTS phonetic-characters property. @type {string} */
AbstractTts.PHONETIC_CHARACTERS = 'phoneticCharacters';
