// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains types related to speech generation.
 */

/**
 * Categories for a speech utterance. This can be used with the
 * CATEGORY_FLUSH queue mode, which flushes all utterances from a given
 * category but not other utterances.
 *
 * NAV: speech related to explicit navigation, or focus changing.
 * LIVE: speech coming from changes to live regions.
 *
 * @enum {string}
 */
export const TtsCategory = {
  LIVE: 'live',
  NAV: 'nav',
};

/**
 * Queue modes for calls to {@code TtsInterface.speak}. The modes are listed in
 * descending order of priority.
 * @enum
 */
export const QueueMode = {
  /**
     Prepend the current utterance (if any) to the queue, stop speech, and
     speak this utterance.
   */
  INTERJECT: 0,

  /** Stop speech, clear everything, then speak this utterance. */
  FLUSH: 1,

  /**
   * Clear any utterances of the same category (as set by
   * properties['category']) from the queue, then enqueue this utterance.
   */
  CATEGORY_FLUSH: 2,

  /** Append this utterance to the end of the queue. */
  QUEUE: 3,
};

/** Structure to store properties around TTS speech production. */
export class TtsSpeechProperties {
  /** @param {Object=} opt_initialValues */
  constructor(opt_initialValues) {
    /** @public {TtsCategory|undefined} */
    this.category;

    /** @public {string|undefined} */
    this.color;

    /** @public {boolean|undefined} */
    this.delay;

    /** @public {boolean|undefined} */
    this.doNotInterrupt;

    /** @public {string|undefined} */
    this.fontWeight;

    /** @public {string|undefined} */
    this.lang;

    /** @public {boolean|undefined} */
    this.math;

    /** @public {boolean|undefined} */
    this.pause;

    /** @public {boolean|undefined} */
    this.phoneticCharacters;

    /** @public {string|undefined} */
    this.punctuationEcho;

    /** @public {boolean|undefined} */
    this.token;

    /** @public {string|undefined} */
    this.voiceName;

    /** @public {number|undefined} */
    this.pitch;
    /** @public {number|undefined} */
    this.relativePitch;

    /** @public {number|undefined} */
    this.rate;
    /** @public {number|undefined} */
    this.relativeRate;

    /** @public {number|undefined} */
    this.volume;
    /** @public {number|undefined} */
    this.relativeVolume;

    /** @public {function()|undefined} */
    this.startCallback;
    /** @public {function(boolean=)|undefined} */
    this.endCallback;

    /** @public {function(Object)|undefined} */
    this.onEvent;

    this.init_(opt_initialValues);
  }

  /** @return {!Object} */
  toJSON() {
    return Object.assign({}, this);
  }

  /**
   * @param {Object=} opt_initialValues
   * @private
   */
  init_(opt_initialValues) {
    if (!opt_initialValues) {
      return;
    }
    Object.assign(this, opt_initialValues);
  }
}

/**
 * A collection of TTS personalities to differentiate text.
 * @type {!Object<!TtsSpeechProperties>}
 */
export const Personality = {
  // TTS personality for annotations - text spoken by ChromeVox that
  // elaborates on a user interface element but isn't displayed on-screen.
  ANNOTATION: new TtsSpeechProperties({
    'relativePitch': -0.25,
    // TODO:(rshearer) Added this color change for I/O presentation.
    'color': 'yellow',
    'punctuationEcho': 'none',
  }),

  // TTS personality for announcements - text spoken by ChromeVox that
  // isn't tied to any user interface elements.
  ANNOUNCEMENT: new TtsSpeechProperties({
    'punctuationEcho': 'none',
  }),

  // TTS personality for an aside - text in parentheses.
  ASIDE: new TtsSpeechProperties({
    'relativePitch': -0.1,
    'color': '#669',
  }),

  // TTS personality for capital letters.
  CAPITAL: new TtsSpeechProperties({
    'relativePitch': 0.2,
  }),

  // TTS personality for deleted text.
  DELETED: new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'relativePitch': -0.6,
  }),

  // TTS personality for dictation hints.
  DICTATION_HINT: new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'relativePitch': 0.3,
  }),

  // TTS personality for emphasis or italicized text.
  EMPHASIS: new TtsSpeechProperties({
    'relativeVolume': 0.1,
    'relativeRate': -0.1,
    'color': '#6bb',
    'fontWeight': 'bold',
  }),

  // TTS personality for quoted text.
  QUOTE: new TtsSpeechProperties({
    'relativePitch': 0.1,
    'color': '#b6b',
    'fontWeight': 'bold',
  }),

  // TTS personality for strong or bold text.
  STRONG: new TtsSpeechProperties({
    'relativePitch': 0.1,
    'color': '#b66',
    'fontWeight': 'bold',
  }),

  // TTS personality for alerts from the system, such as battery level
  // warnings.
  SYSTEM_ALERT: new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'doNotInterrupt': true,
  }),
};

/**
 * Various TTS-related settings keys.
 * @enum {string}
 */
export const TtsSettings = {
  // Color is for the lens display.
  COLOR: 'color',
  FONT_WEIGHT: 'fontWeight',
  LANG: 'lang',
  PAUSE: 'pause',
  PHONETIC_CHARACTERS: 'phoneticCharacters',
  PITCH: 'pitch',
  PUNCTUATION_ECHO: 'punctuationEcho',
  RATE: 'rate',
  RELATIVE_PITCH: 'relativePitch',
  RELATIVE_RATE: 'relativeRate',
  RELATIVE_VOLUME: 'relativeVolume',
  VOLUME: 'volume',
};

/**
 * List of punctuation echoes that the user can cycle through.
 * @type {!Array<{name:(string),
 * msg:(string),
 * regexp:(RegExp),
 * clear:(boolean)}>}
 */
export const PunctuationEchoes = [
  // Punctuation echoed for the 'none' option.
  {
    name: 'none',
    msg: 'no_punctuation',
    regexp: /[-$#"()*;:<>\n\\\/+='~`@_]/g,
    clear: true,
  },

  // Punctuation echoed for the 'some' option.
  {
    name: 'some',
    msg: 'some_punctuation',
    regexp: /[$#"*<>\\\/\{\}+=~`%\u2022\u25e6\u25a0]/g,
    clear: false,
  },

  // Punctuation echoed for the 'all' option.
  {
    name: 'all',
    msg: 'all_punctuation',
    regexp: /[-$#"()*;:<>\n\\\/\{\}\[\]+='~`!@_.,?%\u2022\u25e6\u25a0]/g,
    clear: false,
  },
];

/**
 * Character dictionary. These symbols are replaced with their human readable
 * equivalents. This replacement only occurs for single character utterances.
 * @type {Object<string>}
 */
export const CharacterDictionary = {
  ' ': 'space',
  '\u00a0': 'space',
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
  '\u2022': 'bullet',
  '\u25e6': 'white_bullet',
  '\u25a0': 'square_bullet',
};

/**
 * Substitution dictionary. These symbols or patterns are ALWAYS substituted
 * whenever they occur, so this should be reserved only for unicode characters
 * and characters that never have any different meaning in context.
 *
 * For example, do not include '$' here because $2 should be read as
 * "two dollars".
 * @type {Object<string>}
 */
export const SubstitutionDictionary = {
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
  '\uf8ff': 'apple',
  '£': 'pound sterling',
};
