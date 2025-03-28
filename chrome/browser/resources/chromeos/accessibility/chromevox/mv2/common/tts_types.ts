// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains types related to speech generation.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

/**
 * Categories for a speech utterance. This can be used with the
 * CATEGORY_FLUSH queue mode, which flushes all utterances from a given
 * category but not other utterances.
 *
 * NAV: speech related to explicit navigation, or focus changing.
 * LIVE: speech coming from changes to live regions.
 */
export enum TtsCategory {
  LIVE = 'live',
  NAV = 'nav',
}

/**
 * Queue modes for calls to {@code TtsInterface.speak}. The modes are listed in
 * descending order of priority.
 */
export enum QueueMode {
  /**
   * Prepend the current utterance (if any) to the queue, stop speech, and
   * speak this utterance.
   */
  INTERJECT,

  /** Stop speech, clear everything, then speak this utterance. */
  FLUSH,

  /**
   * Clear any utterances of the same category (as set by
   * properties['category']) from the queue, then enqueue this utterance.
   */
  CATEGORY_FLUSH,

  /** Append this utterance to the end of the queue. */
  QUEUE,
}

/** Structure to store properties around TTS speech production. */
export class TtsSpeechProperties {
  category?: TtsCategory;
  color?: string;
  delay?: boolean;
  doNotInterrupt?: boolean;
  fontWeight?: string;
  lang?: string;
  math?: boolean;
  pause?: boolean;
  phoneticCharacters?: boolean;
  punctuationEcho?: string;
  token?: boolean;
  voiceName?: string;
  pitch?: number;
  relativePitch?: number;
  rate?: number;
  relativeRate?: number;
  volume?: number;
  relativeVolume?: number;

  startCallback?: VoidFunction;
  endCallback?: (val?: boolean) => void;
  onEvent?: (event: Object) => void;

  constructor(initialValues?: Object) {
    this.init_(initialValues);
  }

  toJSON(): Object {
    return Object.assign({}, this);
  }

  private init_(initialValues?: Object): void {
    if (!initialValues) {
      return;
    }
    Object.assign(this, initialValues);
  }
}

/**
 * A collection of TTS personalities to differentiate text.
 * @type {!Object<!TtsSpeechProperties>}
 */
export type Personality = TtsSpeechProperties;
export namespace Personality {
  // TTS personality for annotations - text spoken by ChromeVox that
  // elaborates on a user interface element but isn't displayed on-screen.
  export const ANNOTATION = new TtsSpeechProperties({
    'relativePitch': -0.25,
    // TODO:(rshearer) Added this color change for I/O presentation.
    'color': 'yellow',
    'punctuationEcho': 'none',
  });

  // TTS personality for announcements - text spoken by ChromeVox that
  // isn't tied to any user interface elements.
  export const ANNOUNCEMENT = new TtsSpeechProperties({
    'punctuationEcho': 'none',
  });

  // TTS personality for an aside - text in parentheses.
  export const ASIDE = new TtsSpeechProperties({
    'relativePitch': -0.1,
    'color': '#669',
  });

  // TTS personality for capital letters.
  export const CAPITAL = new TtsSpeechProperties({
    'relativePitch': 0.2,
  });

  // TTS personality for deleted text.
  export const DELETED = new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'relativePitch': -0.6,
  });

  // TTS personality for dictation hints.
  export const DICTATION_HINT = new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'relativePitch': 0.3,
  });

  // TTS personality for emphasis or italicized text.
  export const EMPHASIS = new TtsSpeechProperties({
    'relativeVolume': 0.1,
    'relativeRate': -0.1,
    'color': '#6bb',
    'fontWeight': 'bold',
  });

  // TTS personality for quoted text.
  export const QUOTE = new TtsSpeechProperties({
    'relativePitch': 0.1,
    'color': '#b6b',
    'fontWeight': 'bold',
  });

  // TTS personality for strong or bold text.
  export const STRONG = new TtsSpeechProperties({
    'relativePitch': 0.1,
    'color': '#b66',
    'fontWeight': 'bold',
  });

  // TTS personality for alerts from the system, such as battery level
  // warnings.
  export const SYSTEM_ALERT = new TtsSpeechProperties({
    'punctuationEcho': 'none',
    'doNotInterrupt': true,
  });
}

/** Various TTS-related settings keys. */
export enum TtsSettings {
  // Color is for the lens display.
  COLOR = 'color',
  FONT_WEIGHT = 'fontWeight',
  LANG = 'lang',
  PAUSE = 'pause',
  PHONETIC_CHARACTERS = 'phoneticCharacters',
  PITCH = 'pitch',
  PUNCTUATION_ECHO = 'punctuationEcho',
  RATE = 'rate',
  RELATIVE_PITCH = 'relativePitch',
  RELATIVE_RATE = 'relativeRate',
  RELATIVE_VOLUME = 'relativeVolume',
  VOLUME = 'volume',
}

interface PunctuationEcho {
  name: string;
  msg: string;
  regexp: RegExp;
  clear: boolean;
}

/** List of punctuation echoes that the user can cycle through. */
export const PunctuationEchoes: PunctuationEcho[] = [
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
 */
export const CharacterDictionary: Record<string, string> = {
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
 */
export const SubstitutionDictionary: Record<string, string> = {
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

TestImportManager.exportForTesting(
    ['QueueMode', QueueMode], ['TtsSettings', TtsSettings], TtsSpeechProperties,
    ['TtsCategory', TtsCategory]);
