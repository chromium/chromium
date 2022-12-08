// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This file specifies the glyphs and labels to be applied to keys in the
 * keyboard-diagram component, for all markets in which ChromeOS devices are
 * shipped.
 *
 * Entries in layout descriptions should be ordered by key location, from top to
 * bottom and then left to right.
 *
 * Key labels that are words (such as "shift" or "échap") are not set to
 * localized strings, as the objective here is to match the labels on the
 * physical keyboard in front of the user, not what they would be labelled in
 * the current system language.
 */

const kAzertyLetters = [
  [16, 'a'],
  [17, 'z'],
  [18, 'e'],
  [19, 'r'],
  [20, 't'],
  [21, 'y'],
  [22, 'u'],
  [23, 'i'],
  [24, 'o'],
  [25, 'p'],

  [30, 'q'],
  [31, 's'],
  [32, 'd'],
  [33, 'f'],
  [34, 'g'],
  [35, 'h'],
  [36, 'j'],
  [37, 'k'],
  [38, 'l'],
  [39, 'm'],

  [44, 'w'],
  [45, 'x'],
  [46, 'c'],
  [47, 'v'],
  [48, 'b'],
  [49, 'n'],
];

const kQwertyLetters = [
  [16, 'q'],
  [17, 'w'],
  [18, 'e'],
  [19, 'r'],
  [20, 't'],
  [21, 'y'],
  [22, 'u'],
  [23, 'i'],
  [24, 'o'],
  [25, 'p'],

  [30, 'a'],
  [31, 's'],
  [32, 'd'],
  [33, 'f'],
  [34, 'g'],
  [35, 'h'],
  [36, 'j'],
  [37, 'k'],
  [38, 'l'],

  [44, 'z'],
  [45, 'x'],
  [46, 'c'],
  [47, 'v'],
  [48, 'b'],
  [49, 'n'],
  [50, 'm'],
];

const kQwertzLetters = [
  [16, 'q'],
  [17, 'w'],
  [18, 'e'],
  [19, 'r'],
  [20, 't'],
  [21, 'z'],
  [22, 'u'],
  [23, 'i'],
  [24, 'o'],
  [25, 'p'],

  [30, 'a'],
  [31, 's'],
  [32, 'd'],
  [33, 'f'],
  [34, 'g'],
  [35, 'h'],
  [36, 'j'],
  [37, 'k'],
  [38, 'l'],

  [44, 'y'],
  [45, 'x'],
  [46, 'c'],
  [47, 'v'],
  [48, 'b'],
  [49, 'n'],
  [50, 'm'],
];

const kUsEnglishNoSideLabels = [
  ...kQwertyLetters,
  [41, {bottomLeft: '`', topLeft: '~'}],
  [ 2, {bottomLeft: '1', topLeft: '!'}],
  [ 3, {bottomLeft: '2', topLeft: '@'}],
  [ 4, {bottomLeft: '3', topLeft: '#'}],
  [ 5, {bottomLeft: '4', topLeft: '$'}],
  [ 6, {bottomLeft: '5', topLeft: '%'}],
  [ 7, {bottomLeft: '6', topLeft: '^'}],
  [ 8, {bottomLeft: '7', topLeft: '&'}],
  [ 9, {bottomLeft: '8', topLeft: '*'}],
  [10, {bottomLeft: '9', topLeft: '('}],
  [11, {bottomLeft: '0', topLeft: ')'}],
  [12, {bottomLeft: '-', topLeft: '_'}],
  [13, {bottomLeft: '=', topLeft: '+'}],

  [26, {bottomLeft: '[', topLeft: '{'}],
  [27, {bottomLeft: ']', topLeft: '}'}],
  [43, {bottomLeft: '\\', topLeft: '|'}],

  [39, {bottomLeft: ';', topLeft: ':'}],
  [40, {bottomLeft: '\'', topLeft: '"'}],

  [86, {bottomLeft: '<', topLeft: '>'}],
  [51, {bottomLeft: ',', topLeft: '<'}],
  [52, {bottomLeft: '.', topLeft: '>'}],
  [53, {bottomLeft: '/', topLeft: '?'}],
];

const kUsEnglish = [
  ...kUsEnglishNoSideLabels,
  [14, 'backspace'],

  [15, 'tab'],

  [28, 'enter'],

  [42, 'shift'],
  [54, 'shift'],
];

const kUsEnglishInternational = [
  ...kUsEnglish,
  [6, {bottomLeft: '5', topLeft: '%', bottomRight: '€'}],
  [100, 'alt gr'],
];

const kGbEnglish = [
  ...kUsEnglish,
  [41, {bottomLeft: '`', topLeft: '¬', bottomRight: '¦'}],
  [ 3, {bottomLeft: '2', topLeft: '"'}],
  [ 4, {bottomLeft: '3', topLeft: '£'}],
  [ 5, {bottomLeft: '4', topLeft: '$', bottomRight: '€'}],

  [40, {bottomLeft: '\'', topLeft: '@'}],
  [43, {bottomLeft: '#', topLeft: '~'}],

  [86, {bottomLeft: '\\', topLeft: '|'}],

  [100, 'alt gr'],
];

const kLatamSpanish = [
  ...kQwertyLetters,
  [41, {bottomLeft: '|', topLeft: '°', bottomRight: '¬'}],
  [ 2, {bottomLeft: '1', topLeft: '!'}],
  [ 3, {bottomLeft: '2', topLeft: '"'}],
  [ 4, {bottomLeft: '3', topLeft: '#'}],
  [ 5, {bottomLeft: '4', topLeft: '$'}],
  [ 6, {bottomLeft: '5', topLeft: '%'}],
  [ 7, {bottomLeft: '6', topLeft: '&'}],
  [ 8, {bottomLeft: '7', topLeft: '/'}],
  [ 9, {bottomLeft: '8', topLeft: '('}],
  [10, {bottomLeft: '9', topLeft: ')'}],
  [11, {bottomLeft: '0', topLeft: '='}],
  [12, {bottomLeft: '\'', topLeft: '?', bottomRight: '\\'}],
  [13, {bottomLeft: '¿', topLeft: '¡'}],

  [16, {main: 'q', bottomRight: '@'}],
  [18, {main: 'e', bottomRight: '€'}],
  [26, {bottomLeft: '◌́', topLeft: '◌̈'}],
  [27, {bottomLeft: '+', topLeft: '*', bottomRight: '~'}],
  [28, 'intro'],

  [39, 'ñ'],
  [40, {bottomLeft: '{', topLeft: '[', bottomRight: '◌̂'}],
  [43, {bottomLeft: '}', topLeft: ']', bottomRight: '◌̂'}],

  [42, 'mayús'],
  [86, {bottomLeft: '<', topLeft: '>'}],
  [51, {bottomLeft: ',', topLeft: ';'}],
  [52, {bottomLeft: '.', topLeft: ':'}],
  [53, {bottomLeft: '-', topLeft: '_'}],
  [54, 'mayús'],

  [100, 'alt gr'],
];

const kBrPortuguese = [
  /*
   * Depending on the variant, this layout might have symbols or text labels on
   * Tab, Shift, and Backspace. Since there is no way for code to distinguish
   * between them, err on the side of symbols.
   */
  ...kQwertyLetters,
  [41, {bottomLeft: '\'', topLeft: '"'}],
  [2, {bottomLeft: '1', topLeft: '!', bottomRight: '¹'}],
  [3, {bottomLeft: '2', topLeft: '@', bottomRight: '²'}],
  [4, {bottomLeft: '3', topLeft: '#', bottomRight: '³'}],
  [5, {bottomLeft: '4', topLeft: '$', bottomRight: '£'}],
  [6, {bottomLeft: '5', topLeft: '%', bottomRight: '¢'}],
  [7, {bottomLeft: '6', topLeft: '◌̈', bottomRight: '¬'}],
  [8, {bottomLeft: '7', topLeft: '&'}],
  [9, {bottomLeft: '8', topLeft: '*'}],
  [10, {bottomLeft: '9', topLeft: '('}],
  [11, {bottomLeft: '0', topLeft: ')'}],
  [12, {bottomLeft: '-', topLeft: '_'}],
  [13, {bottomLeft: '=', topLeft: '+', bottomRight: '§'}],

  [16, {main: 'q', bottomRight: '/'}],
  [17, {main: 'w', bottomRight: '?'}],
  [18, {main: 'e', bottomRight: '°'}],
  [26, {bottomLeft: '◌́', topLeft: '◌̀'}],
  [27, {bottomLeft: '[', topLeft: '{', bottomRight: 'ª'}],

  [39, 'ç'],
  [40, {bottomLeft: '~', topLeft: '◌̂'}],
  [43, {bottomLeft: ']', topLeft: '}', bottomRight: 'º'}],

  [86, {bottomLeft: '\\', topLeft: '|'}],
  [51, {bottomLeft: ',', topLeft: '<'}],
  [52, {bottomLeft: '.', topLeft: '>'}],
  [53, {bottomLeft: ';', topLeft: ':'}],

  [100, 'alt gr'],
];

const kCaFrench = [
  ...kQwertyLetters,
  [41, {bottomLeft: '◌̀', topLeft: '~', bottomRight: '#', topRight: '|'}],
  [ 2, {bottomLeft: '1', topLeft: '!', bottomRight: '±', topRight: '"'}],
  [ 3, {bottomLeft: '2', topLeft: '@', bottomRight: '@', topRight: '/'}],
  [ 4, {bottomLeft: '3', topLeft: '#', bottomRight: '£'}],
  [ 5, {bottomLeft: '4', topLeft: '$', bottomRight: '¢'}],
  [ 6, {bottomLeft: '5', topLeft: '%', bottomRight: '¤'}],
  [ 7, {bottomLeft: '6', topLeft: '◌̂', bottomRight: '¬', topRight: '?'}],
  [ 8, {bottomLeft: '7', topLeft: '&', bottomRight: '¦'}],
  [ 9, {bottomLeft: '8', topLeft: '*', bottomRight: '²'}],
  [10, {bottomLeft: '9', topLeft: '(', bottomRight: '³'}],
  [11, {bottomLeft: '0', topLeft: ')', bottomRight: '¼'}],
  [12, {bottomLeft: '-', topLeft: '_', bottomRight: '½'}],
  [13, {bottomLeft: '=', topLeft: '+', bottomRight: '¾'}],

  [24, {main: 'o', bottomRight: '§'}],
  [25, {main: 'p', bottomRight: '¶'}],
  [26, {bottomLeft: '[', topLeft: '{', bottomRight: '◌̂'}],
  [27, {bottomLeft: ']', topLeft: '}', bottomRight: '◌̧'}],

  [39, {bottomLeft: ';', topLeft: ':', bottomRight: '~'}],
  [40, {bottomLeft: '\'', topLeft: '"', bottomRight: '◌̀'}],
  [43, {bottomLeft: '\\', topLeft: '|', bottomRight: '<', topRight: '>'}],

  [86, {bottomLeft: '\\', topLeft: '|', bottomRight: '«', topRight: '»'}],
  [50, {main: 'm', bottomRight: 'µ'}],
  [51, {bottomLeft: ',', topLeft: '<', topRight: '\''}],
  [52, {bottomLeft: '.', topLeft: '>', topRight: ' '}],
  [53, {bottomLeft: '/', topLeft: '?', bottomRight: 'é'}],

  [100, 'alt gr'],
];

const kRoRomanian = [
  ...kUsEnglishNoSideLabels,
  [41, {bottomLeft: '◌̀', topLeft: '~'}],
  [ 7, {bottomLeft: '6', topLeft: '◌̂'}],

  [16, {main: 'q', bottomRight: 'â'}],
  [20, {main: 't', bottomRight: 'ț'}],
  [23, {main: 'i', bottomRight: 'î'}],

  [30, {main: 'a', bottomRight: 'ă'}],
  [31, {main: 's', bottomRight: 'ș'}],

  [100, 'alt gr'],
];

const kNordic = [
  // Note: this layout has some keys with more glyphs than can be represented
  // by keyboard-key (e.g. specifically the key below escape, the one to the
  // left of backspace, and the ISO key). In those cases the glyphs on the
  // left of the key have been omitted.
  ...kQwertyLetters,
  [41, {bottomLeft: '½', topLeft: '§', bottomRight: '|'}],
  [ 2, {bottomLeft: '1', topLeft: '!'}],
  [ 3, {bottomLeft: '2', topLeft: '"', bottomRight: '@'}],
  [ 4, {bottomLeft: '3', topLeft: '#', bottomRight: '£'}],
  [ 5, {bottomLeft: '4', topLeft: '¤', bottomRight: '$'}],
  [ 6, {bottomLeft: '5', topLeft: '%', bottomRight: '€'}],
  [ 7, {bottomLeft: '6', topLeft: '&'}],
  [ 8, {bottomLeft: '7', topLeft: '/', bottomRight: '{'}],
  [ 9, {bottomLeft: '8', topLeft: '(', bottomRight: '['}],
  [10, {bottomLeft: '9', topLeft: ')', bottomRight: ']'}],
  [11, {bottomLeft: '0', topLeft: '=', bottomRight: '}'}],
  [12, {bottomLeft: '+', topLeft: '?', bottomRight: '\\'}],
  [13, {bottomLeft: '◌́', topLeft: '◌̀', bottomRight: '|'}],

  [26, 'å'],
  [27, {bottomLeft: '◌̈', topLeft: '◌̂', bottomRight: '~'}],

  [39, {bottomLeft: 'æ', topLeft: 'ö', topRight: 'ø'}],
  [40, {bottomLeft: 'ø', topLeft: 'ä', topRight: 'æ'}],
  [43, {bottomLeft: '\'', topLeft: '*'}],

  [86, {bottomLeft: '\\', topLeft: '>', bottomRight: '|'}],
  [51, {bottomLeft: ',', topLeft: ';'}],
  [52, {bottomLeft: '.', topLeft: ':'}],
  [53, {bottomLeft: '-', topLeft: '_'}],

  [100, 'alt gr'],
];

const kDeGerman = [
  ...kQwertzLetters,
  [41, {bottomLeft: '◌̂', topLeft: '°'}],
  [ 2, {bottomLeft: '1', topLeft: '!'}],
  [ 3, {bottomLeft: '2', topLeft: '"', bottomRight: '²'}],
  [ 4, {bottomLeft: '3', topLeft: '§', bottomRight: '³'}],
  [ 5, {bottomLeft: '4', topLeft: '$'}],
  [ 6, {bottomLeft: '5', topLeft: '%'}],
  [ 7, {bottomLeft: '6', topLeft: '&'}],
  [ 8, {bottomLeft: '7', topLeft: '/', bottomRight: '{'}],
  [ 9, {bottomLeft: '8', topLeft: '(', bottomRight: '['}],
  [10, {bottomLeft: '9', topLeft: ')', bottomRight: ']'}],
  [11, {bottomLeft: '0', topLeft: '=', bottomRight: '}'}],
  [12, {bottomLeft: 'ß', topLeft: '?', bottomRight: '\\'}],
  [13, {bottomLeft: '◌́', topLeft: '◌̀'}],

  [16, {main: 'q', bottomRight: '@'}],
  [18, {main: 'e', bottomRight: '€'}],
  [26, 'ü'],
  [27, {bottomLeft: '+', topLeft: '*', bottomRight: '~'}],

  [39, 'ö'],
  [40, 'ä'],
  [43, {bottomLeft: '#', topLeft: '\''}],

  [50, {main: 'm', bottomRight: 'µ'}],
  [51, {bottomLeft: ',', topLeft: ';'}],
  [52, {bottomLeft: '.', topLeft: ':'}],
  [53, {bottomLeft: '-', topLeft: '_'}],

  [86, {bottomLeft: '<', topLeft: '>', bottomRight: '|'}],

  [29, 'strg'],
  [100, 'alt gr'],
  [97, 'strg'],
];

const kArabic = [
  [41, {bottomLeft: '◌̀', topLeft: '~', bottomRight: 'ذ', topRight: '◌ّ'}],
  [ 2, {bottomLeft: '1', topLeft: '!', bottomRight: '١'}],
  [ 3, {bottomLeft: '2', topLeft: '@', bottomRight: '٢'}],
  [ 4, {bottomLeft: '3', topLeft: '#', bottomRight: '٣'}],
  [ 5, {bottomLeft: '4', topLeft: '$', bottomRight: '٤'}],
  [ 6, {bottomLeft: '5', topLeft: '%', bottomRight: '٥'}],
  [ 7, {bottomLeft: '6', topLeft: '◌̂', bottomRight: '٦'}],
  [ 8, {bottomLeft: '7', topLeft: '&', bottomRight: '٧'}],
  [ 9, {bottomLeft: '8', topLeft: '*', bottomRight: '٨'}],
  [10, {bottomLeft: '9', topLeft: '(', bottomRight: '٩'}],
  [11, {bottomLeft: '0', topLeft: ')', bottomRight: '٠'}],
  [12, {bottomLeft: '-', topLeft: '_', bottomRight: ' '}],
  [13, {bottomLeft: '=', topLeft: '+', bottomRight: ' '}],

  [16, {bottomLeft: 'q', bottomRight: 'ض', topRight: '◌َ'}],
  [17, {bottomLeft: 'w', bottomRight: 'ص', topRight: '◌ً'}],
  [18, {bottomLeft: 'e', bottomRight: 'ث', topRight: '◌ُ'}],
  [19, {bottomLeft: 'r', bottomRight: 'ق', topRight: '◌ٌ'}],
  [20, {bottomLeft: 't', bottomRight: 'ف', topRight: 'لإ'}],
  [21, {bottomLeft: 'y', bottomRight: 'غ', topRight: 'إ'}],
  [22, {bottomLeft: 'u', bottomRight: 'ع', topRight: '‘'}],
  [23, {bottomLeft: 'i', bottomRight: 'ه', topRight: '÷'}],
  [24, {bottomLeft: 'o', bottomRight: 'خ', topRight: '×'}],
  [25, {bottomLeft: 'p', bottomRight: 'ح', topRight: '؛'}],
  [26, {bottomLeft: '[', topLeft: '{', bottomRight: 'ج', topRight: '<'}],
  [27, {bottomLeft: ']', topLeft: '}', bottomRight: 'د', topRight: '>'}],
  [43, {bottomLeft: '\\', topLeft: '|'}],

  [30, {bottomLeft: 'a', bottomRight: 'ش', topRight: '◌ِ'}],
  [31, {bottomLeft: 's', bottomRight: 'س', topRight: '◌ٍ'}],
  [32, {bottomLeft: 'd', bottomRight: 'ي', topRight: '['}],
  [33, {bottomLeft: 'f', bottomRight: 'ب', topRight: ']'}],
  [34, {bottomLeft: 'g', bottomRight: 'ل', topRight: 'لأ'}],
  [35, {bottomLeft: 'h', bottomRight: 'ا', topRight: 'أ'}],
  [36, {bottomLeft: 'j', bottomRight: 'ت', topRight: 'ـ'}],
  [37, {bottomLeft: 'k', bottomRight: 'ن', topRight: '،'}],
  [38, {bottomLeft: 'l', bottomRight: 'م', topRight: '/'}],
  [39, {bottomLeft: ';', topLeft: ':', bottomRight: 'ك'}],
  [40, {bottomLeft: '\'', topLeft: '"', bottomRight: 'ط'}],

  [44, {bottomLeft: 'z', bottomRight: 'ئ', topRight: '~'}],
  [45, {bottomLeft: 'x', bottomRight: 'ء', topRight: '°'}],
  [46, {bottomLeft: 'c', bottomRight: 'ؤ', topRight: '}'}],
  [47, {bottomLeft: 'v', bottomRight: 'ر', topRight: '{'}],
  [48, {bottomLeft: 'b', bottomRight: 'لا', topRight: 'لآ'}],
  [49, {bottomLeft: 'n', bottomRight: 'ى', topRight: 'آ'}],
  [50, {bottomLeft: 'm', bottomRight: 'ة', topRight: '\''}],
  [51, {bottomLeft: ',', topLeft: '<', bottomRight: 'و', topRight: ','}],
  [52, {bottomLeft: '.', topLeft: '>', bottomRight: 'ز', topRight: '.'}],
  [53, {bottomLeft: '/', topLeft: '?', bottomRight: 'ظ', topRight: '؟'}],
];

const kTraditionalChinese = [
  [41, {bottomLeft: '`', topLeft: '~'}],
  [ 2, {bottomLeft: '1', topLeft: '!', topRight: 'ㄅ'}],
  [ 3, {bottomLeft: '2', topLeft: '@', topRight: 'ㄉ'}],
  [ 4, {bottomLeft: '3', topLeft: '#', topRight: '◌̌'}],
  [ 5, {bottomLeft: '4', topLeft: '$', topRight: '◌̀'}],
  [ 6, {bottomLeft: '5', topLeft: '%', topRight: 'ㄓ'}],
  [ 7, {bottomLeft: '6', topLeft: '◌̂', topRight: '◌́'}],
  [ 8, {bottomLeft: '7', topLeft: '&', topRight: '◌̇'}],
  [ 9, {bottomLeft: '8', topLeft: '*', topRight: 'ㄚ'}],
  [10, {bottomLeft: '9', topLeft: '(', topRight: 'ㄞ'}],
  [11, {bottomLeft: '0', topLeft: ')', topRight: 'ㄢ'}],
  [12, {bottomLeft: '-', topLeft: '_', topRight: '儿'}],
  [13, {bottomLeft: '=', topLeft: '+'}],
  [14, 'backspace'],

  [15, 'tab'],
  [16, {bottomLeft: 'q', bottomRight: '手', topRight: 'ㄆ'}],
  [17, {bottomLeft: 'w', bottomRight: '田', topRight: 'ㄊ'}],
  [18, {bottomLeft: 'e', bottomRight: '水', topRight: 'ㄍ'}],
  [19, {bottomLeft: 'r', bottomRight: '口', topRight: 'ㄐ'}],
  [20, {bottomLeft: 't', bottomRight: '廿', topRight: 'ㄔ'}],
  [21, {bottomLeft: 'y', bottomRight: '卜', topRight: 'ㄗ'}],
  [22, {bottomLeft: 'u', bottomRight: '山', topRight: 'ㄧ'}],
  [23, {bottomLeft: 'i', bottomRight: '戈', topRight: 'ㄛ'}],
  [24, {bottomLeft: 'o', bottomRight: '人', topRight: 'ㄟ'}],
  [25, {bottomLeft: 'p', bottomRight: '心', topRight: 'ㄣ'}],
  [26, {bottomLeft: '[', topLeft: '{'}],
  [27, {bottomLeft: ']', topLeft: '}'}],
  [43, {bottomLeft: '\\', topLeft: '|', bottomRight: '₩'}],

  [30, {bottomLeft: 'a', bottomRight: '日', topRight: 'ㄇ'}],
  [31, {bottomLeft: 's', bottomRight: '尸', topRight: 'ㄋ'}],
  [32, {bottomLeft: 'd', bottomRight: '木', topRight: 'ㄎ'}],
  [33, {bottomLeft: 'f', bottomRight: '火', topRight: 'ㄑ'}],
  [34, {bottomLeft: 'g', bottomRight: '土', topRight: 'ㄕ'}],
  [35, {bottomLeft: 'h', bottomRight: '竹', topRight: 'ㄘ'}],
  [36, {bottomLeft: 'j', bottomRight: '十', topRight: 'ㄨ'}],
  [37, {bottomLeft: 'k', bottomRight: '大', topRight: 'ㄜ'}],
  [38, {bottomLeft: 'l', bottomRight: '中', topRight: 'ㄠ'}],
  [39, {bottomLeft: ';', topLeft: ':', topRight: 'ㄤ'}],
  [40, {bottomLeft: '\'', topLeft: '"'}],

  [42, 'shift'],
  [44, {bottomLeft: 'z', bottomRight: '重', topRight: 'ㄈ'}],
  [45, {bottomLeft: 'x', bottomRight: '難', topRight: 'ㄌ'}],
  [46, {bottomLeft: 'c', bottomRight: '金', topRight: 'ㄏ'}],
  [47, {bottomLeft: 'v', bottomRight: '女', topRight: 'ㄒ'}],
  [48, {bottomLeft: 'b', bottomRight: '月', topRight: 'ㄖ'}],
  [49, {bottomLeft: 'n', bottomRight: '弓', topRight: 'ㄙ'}],
  [50, {bottomLeft: 'm', bottomRight: '一', topRight: 'ㄩ'}],
  [51, {bottomLeft: ',', topLeft: '<', topRight: 'ㄝ'}],
  [52, {bottomLeft: '.', topLeft: '>', topRight: 'ㄡ'}],
  [53, {bottomLeft: '/', topLeft: '?', topRight: 'ㄥ'}],
  [54, 'shift'],
];

const kSpainSpanish = [
  ...kQwertyLetters,
  [41, {bottomLeft: 'º', topLeft: 'ª', bottomRight: '\\'}],
  [2, {bottomLeft: '1', topLeft: '!', bottomRight: '|'}],
  [3, {bottomLeft: '2', topLeft: '"', bottomRight: '@'}],
  [4, {bottomLeft: '3', topLeft: '·', bottomRight: '#'}],
  [5, {bottomLeft: '4', topLeft: '$', bottomRight: '~'}],
  [6, {bottomLeft: '5', topLeft: '%'}],
  [7, {bottomLeft: '6', topLeft: '&', bottomRight: '¬'}],
  [8, {bottomLeft: '7', topLeft: '/'}],
  [9, {bottomLeft: '8', topLeft: '('}],
  [10, {bottomLeft: '9', topLeft: ')'}],
  [11, {bottomLeft: '0', topLeft: '='}],
  [12, {bottomLeft: '\'', topLeft: '?'}],
  [13, {bottomLeft: '¡', topLeft: '¿'}],

  [18, {main: 'e', bottomRight: '€'}],
  [26, {bottomLeft: '◌̀', topLeft: '◌̂'}],
  [27, {bottomLeft: '+', topLeft: '*', bottomRight: '~'}],
  [28, 'intro'],

  [39, 'ñ'],
  [40, {bottomLeft: '◌́', topLeft: '◌̈', bottomRight: '{'}],
  [43, {main: 'ç', bottomRight: '◌̂'}],

  [42, 'mayús'],
  [86, {bottomLeft: '<', topLeft: '>'}],
  [51, {bottomLeft: ',', topLeft: ';'}],
  [52, {bottomLeft: '.', topLeft: ':'}],
  [53, {bottomLeft: '-', topLeft: '_'}],
  [54, 'mayús'],

  [100, 'alt gr'],
];

/**
 * A hard-coded collection of glyphs to be shown on keys for a given region
 * code. Region codes are taken from a table in the factory docs [0]. For each
 * region code, the object contains pairs of evdev codes and glyphs or glyph
 * sets, which are applied to the keyboard diagram when it loads. See the KEY_
 * constants in input-event-codes.h in the Linux Kernel sources [1] for the
 * evdev codes, and the file overview comment in keyboard_key.js for how the
 * glyphs are laid out.
 *
 * [0]:
 * https://storage.googleapis.com/chromeos-factory-docs/sdk/regions.html#available-regions
 * [1]:
 * https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/kernel/upstream/include/uapi/linux/input-event-codes.h
 *
 * @const {!Object<string, !Array<!Array<(number|string|!{
 *   main: ?string,
 *   icon: ?string,
 *   topLeft: ?string,
 *   topRight: ?string,
 *   bottomLeft: ?string,
 *   bottomRight: ?string,
 *   ariaNameI18n: ?string,
 * })>>>}
 */
const kLayouts = {
  /* United Arab Emirates */
  'ae': kUsEnglish,
  /* Argentina */
  'ar': kLatamSpanish,
  /* Austria */
  'at': kDeGerman,
  /* Australia */
  'au': kUsEnglish,
  /* Belgium */
  'be': [
    ...kAzertyLetters,
    [41, {bottomLeft: '²', topLeft: '³'}],
    [2, {bottomLeft: '&', topLeft: '1', bottomRight: '|'}],
    [3, {bottomLeft: 'é', topLeft: '2', bottomRight: '@'}],
    [4, {bottomLeft: '"', topLeft: '3', bottomRight: '#'}],
    [5, {bottomLeft: '\'', topLeft: '4'}],
    [6, {bottomLeft: '(', topLeft: '5'}],
    [7, {bottomLeft: '§', topLeft: '6', bottomRight: '◌̂'}],
    [8, {bottomLeft: 'è', topLeft: '7'}],
    [9, {bottomLeft: '!', topLeft: '8'}],
    [10, {bottomLeft: 'ç', topLeft: '9', bottomRight: '{'}],
    [11, {bottomLeft: 'à', topLeft: '0', bottomRight: '}'}],
    [12, {bottomLeft: ')', topLeft: '°'}],
    [13, {bottomLeft: '-', topLeft: '_'}],

    [26, {bottomLeft: '◌̂', topLeft: '◌̈', bottomRight: '['}],
    [27, {bottomLeft: '$', topLeft: '*', bottomRight: ']'}],

    [40, {bottomLeft: 'ù', topLeft: '%', bottomRight: '◌́'}],
    [43, {bottomLeft: 'µ', topLeft: '£', bottomRight: '◌̀'}],

    [86, {bottomLeft: '<', topLeft: '>', bottomRight: '\\'}],
    [50, {bottomLeft: ',', topLeft: '?'}],
    [51, {bottomLeft: ';', topLeft: '.'}],
    [52, {bottomLeft: ':', topLeft: '/'}],
    [53, {bottomLeft: '=', topLeft: '+', bottomRight: '~'}],

    [100, 'alt gr'],
  ],
  /* Bulgaria */
  'bg': [
    [41, {bottomLeft: '`', topLeft: '~', topRight: ' '}],
    [2, {bottomLeft: '1', topLeft: '!', topRight: ' '}],
    [3, {bottomLeft: '2', topLeft: '@', topRight: '?'}],
    [4, {bottomLeft: '3', topLeft: '#', topRight: '+'}],
    [5, {bottomLeft: '4', topLeft: '$', topRight: '"'}],
    [6, {bottomLeft: '5', topLeft: '%', topRight: ' '}],
    [7, {bottomLeft: '6', topLeft: '^', topRight: '='}],
    [8, {bottomLeft: '7', topLeft: '&', topRight: ':'}],
    [9, {bottomLeft: '8', topLeft: '*', topRight: '/'}],
    [10, {bottomLeft: '9', topLeft: '(', topRight: '_'}],
    [11, {bottomLeft: '0', topLeft: ')', topRight: '№'}],
    [12, {bottomLeft: '-', topLeft: '_', topRight: '|'}],
    [13, {bottomLeft: '=', topLeft: '+', topRight: 'V', bottomRight: '.'}],
    [14, 'backspace'],

    [15, 'tab'],
    [16, {topLeft: 'q', bottomRight: ',', topRight: 'ы'}],
    [17, {topLeft: 'w', bottomRight: 'у'}],
    [18, {topLeft: 'e', bottomRight: 'е'}],
    [19, {topLeft: 'r', bottomRight: 'и'}],
    [20, {topLeft: 't', bottomRight: 'ш'}],
    [21, {topLeft: 'y', bottomRight: 'щ'}],
    [22, {topLeft: 'u', bottomRight: 'к'}],
    [23, {topLeft: 'i', bottomRight: 'с'}],
    [24, {topLeft: 'o', bottomRight: 'д'}],
    [25, {topLeft: 'p', bottomRight: 'з'}],
    [26, {bottomLeft: '[', topLeft: '{', bottomRight: 'ц'}],
    [27, {bottomLeft: ']', topLeft: '}', topRight: '§', bottomRight: ';'}],
    [43, {bottomLeft: '\\', topLeft: '|', topRight: '(', bottomRight: ')'}],

    [30, {topLeft: 'a', bottomRight: 'ь', topRight: 'ѝ'}],
    [31, {topLeft: 's', bottomRight: 'я'}],
    [32, {topLeft: 'd', bottomRight: 'а'}],
    [33, {topLeft: 'f', bottomRight: 'о'}],
    [34, {topLeft: 'g', bottomRight: 'ж'}],
    [35, {topLeft: 'h', bottomRight: 'г'}],
    [36, {topLeft: 'j', bottomRight: 'т'}],
    [37, {topLeft: 'k', bottomRight: 'н'}],
    [38, {topLeft: 'l', bottomRight: 'в'}],
    [39, {bottomLeft: ';', topLeft: ':', bottomRight: 'м'}],
    [40, {bottomLeft: '\'', topLeft: '"', bottomRight: 'ч'}],
    [28, 'enter'],

    [42, 'shift'],
    [44, {topLeft: 'z', bottomRight: 'ю'}],
    [45, {topLeft: 'x', bottomRight: 'й'}],
    [46, {topLeft: 'c', bottomRight: 'ъ'}],
    [47, {topLeft: 'v', bottomRight: 'э'}],
    [48, {topLeft: 'b', bottomRight: 'ф'}],
    [49, {topLeft: 'n', bottomRight: 'х'}],
    [50, {topLeft: 'm', bottomRight: 'п'}],
    [51, {bottomLeft: ',', topLeft: '<', bottomRight: 'р'}],
    [52, {bottomLeft: '.', topLeft: '>', bottomRight: 'л'}],
    [53, {bottomLeft: '/', topLeft: '?', bottomRight: 'б'}],
    [54, 'shift'],
  ],
  /* Bahrain */
  'bh': kArabic,
  /* Brazil (ABNT2) */
  'br': kBrPortuguese,
  /* Brazil (ABNT) */
  'br.abnt': kBrPortuguese,
  /* Brazil (US Intl) */
  'br.usintl': kUsEnglishInternational,
  /* Canada (US keyboard) */
  'ca.ansi': kUsEnglish,
  /* Canada (French keyboard) */
  'ca.fr': kCaFrench,
  /* Canada (Hybrid ISO Keyboard) */
  'ca.hybrid': kCaFrench,
  /* Canada (Hybrid Ansi keyboard) */
  'ca.hybridansi': kCaFrench,
  /* Canada (Multilingual ISO, Probably not in use) */
  'ca.multix': kCaFrench,
  /* Switzerland */
  'ch': [
    ...kQwertzLetters,
    [41, {bottomLeft: '§', topLeft: '°'}],
    [2, {bottomLeft: '1', topLeft: '+', bottomRight: '¦'}],
    [3, {bottomLeft: '2', topLeft: '"', bottomRight: '@'}],
    [4, {bottomLeft: '3', topLeft: '*', bottomRight: '#'}],
    [5, {bottomLeft: '4', topLeft: 'ç'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&', bottomRight: '¬'}],
    [8, {bottomLeft: '7', topLeft: '/', bottomRight: '|'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: '¢'}],
    [10, {bottomLeft: '9', topLeft: ')'}],
    [11, {bottomLeft: '0', topLeft: '='}],
    [12, {bottomLeft: '\'', topLeft: '?', bottomRight: '◌́'}],
    [13, {bottomLeft: '◌̂', topLeft: '◌̀', bottomRight: '~'}],
    [14, 'backspace'],

    [15, 'tab'],
    [18, {main: 'e', bottomRight: '€'}],
    [26, {main: 'ü è', bottomRight: '['}],
    [27, {bottomLeft: '◌̈', topLeft: '!', bottomRight: ']'}],

    [39, 'ö é'],
    [40, {main: 'ä à', bottomRight: '{'}],
    [43, {bottomLeft: '$', topLeft: '£', bottomRight: '}'}],

    [42, 'shift'],
    [86, {bottomLeft: '<', topLeft: '>', bottomRight: '\\'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],
    [54, 'shift'],

    [100, 'alt gr'],
  ],
  /* Switzerland (US Intl) */
  'ch.usintl': kUsEnglishInternational,
  /* Chile */
  'cl': kLatamSpanish,
  /* Colombia */
  'co': kLatamSpanish,
  /* Czech Republic */
  'cz': [
    ...kQwertzLetters,
    [41, {bottomLeft: ';', topLeft: '°'}],
    [2, {bottomLeft: '+', topLeft: '1', bottomRight: '!'}],
    [3, {bottomLeft: 'ě', topLeft: '2', bottomRight: '@'}],
    [4, {bottomLeft: 'š', topLeft: '3', bottomRight: '#'}],
    [5, {bottomLeft: 'č', topLeft: '4', bottomRight: '$'}],
    [6, {bottomLeft: 'ř', topLeft: '5', bottomRight: '%'}],
    [7, {bottomLeft: 'ž', topLeft: '6', bottomRight: '^'}],
    [8, {bottomLeft: 'ý', topLeft: '7', bottomRight: '&'}],
    [9, {bottomLeft: 'á', topLeft: '8', bottomRight: '*'}],
    [10, {bottomLeft: 'í', topLeft: '9', bottomRight: '{'}],
    [11, {bottomLeft: 'é', topLeft: '0', bottomRight: '}'}],
    [12, {bottomLeft: '=', topLeft: '%', bottomRight: '\\'}],
    [13, {bottomLeft: '◌́', topLeft: '◌̌'}],
    [14, 'backspace'],

    [15, 'tab'],
    [18, {main: 'e', bottomRight: '€'}],
    [26, {bottomLeft: 'ú', topLeft: '/', bottomRight: '['}],
    [27, {bottomLeft: ')', topLeft: '(', bottomRight: ']'}],

    [39, {bottomLeft: 'ů', topLeft: '¨'}],
    [40, {bottomLeft: '§', topLeft: '!', bottomRight: '\''}],
    [43, {topLeft: '\'', bottomRight: '\\'}],

    [42, 'shift'],
    [86, {bottomLeft: '\\', topLeft: '|', bottomRight: '/'}],
    [51, {bottomLeft: ',', topLeft: '?', bottomRight: '<'}],
    [52, {bottomLeft: '.', topLeft: ':', bottomRight: '>'}],
    [53, {bottomLeft: '-', topLeft: '_', bottomRight: '*'}],
    [54, 'shift'],

    [100, 'alt gr'],
  ],
  /* Germany */
  'de': kDeGerman,
  /* Denmark */
  'dk': kNordic,
  /* Estonia */
  'ee': [
    ...kQwertyLetters,
    [41, {bottomLeft: '◌̌', topLeft: '~'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '"', bottomRight: '@'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: '£'}],
    [5, {bottomLeft: '4', topLeft: '¤', bottomRight: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '/', bottomRight: '{'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: '['}],
    [10, {bottomLeft: '9', topLeft: ')', bottomRight: ']'}],
    [11, {bottomLeft: '0', topLeft: '=', bottomRight: '}'}],
    [12, {bottomLeft: '+', topLeft: '?', bottomRight: '\\'}],
    [13, {bottomLeft: '◌́', topLeft: '◌̀'}],

    [18, {main: 'e', bottomRight: '€'}],
    [26, 'ü'],
    [27, {main: 'õ', bottomRight: '§'}],

    [31, {main: 's', bottomRight: 'š'}],
    [39, 'ö'],
    [40, {main: 'ä', bottomRight: '◌̂'}],
    [43, {bottomLeft: '\'', topLeft: '*', bottomRight: '½'}],

    [86, {bottomLeft: '<', topLeft: '>', bottomRight: '|'}],
    [44, {main: 'z', bottomRight: 'ž'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],

    [100, 'alt gr'],
  ],
  /* Spain */
  'es': kSpainSpanish,
  /* Finland */
  'fi': kNordic,
  /* France */
  'fr': [
    ...kAzertyLetters,
    [1, 'échap'],

    [41, {bottomLeft: '²'}],
    [2, {bottomLeft: '&', topLeft: '1'}],
    [3, {bottomLeft: 'é', topLeft: '2'}],
    [4, {bottomLeft: '"', topLeft: '3'}],
    [5, {bottomLeft: '\'', topLeft: '4'}],
    [6, {bottomLeft: '(', topLeft: '5'}],
    [7, {bottomLeft: '-', topLeft: '6'}],
    [8, {bottomLeft: 'è', topLeft: '7'}],
    [9, {bottomLeft: '_', topLeft: '8'}],
    [10, {bottomLeft: 'ç', topLeft: '9'}],
    [11, {bottomLeft: 'à', topLeft: '0'}],
    [12, {bottomLeft: ')', topLeft: '°'}],
    [13, {bottomLeft: '=', topLeft: '+'}],

    [26, {bottomLeft: '◌̂', topLeft: '◌̈'}],
    [27, {bottomLeft: '$', topLeft: '£'}],

    [40, {bottomLeft: 'ù', topLeft: '%'}],
    [43, {bottomLeft: '*', topLeft: 'µ'}],

    [86, {bottomLeft: '<', topLeft: '>'}],
    [50, {bottomLeft: ',', topLeft: '?'}],
    [51, {bottomLeft: ';', topLeft: '.'}],
    [52, {bottomLeft: ':', topLeft: '/'}],
    [53, {bottomLeft: '!', topLeft: '§'}],

    [100, 'alt gr'],
  ],
  /* United Kingdom */
  'gb': kGbEnglish,
  /* United Kingdom (US extended keyboard) */
  'gb.usext': kUsEnglishInternational,
  /* Gulf Cooperation Council (GCC) */
  'gcc': kArabic,
  /* Greece */
  'gr': [
    [41, {bottomLeft: '◌̀', topLeft: '~'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '@'}],
    [4, {bottomLeft: '3', topLeft: '#'}],
    [5, {bottomLeft: '4', topLeft: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '◌̂'}],
    [8, {bottomLeft: '7', topLeft: '&'}],
    [9, {bottomLeft: '8', topLeft: '*'}],
    [10, {bottomLeft: '9', topLeft: '('}],
    [11, {bottomLeft: '0', topLeft: ')'}],
    [12, {bottomLeft: '-', topLeft: '_'}],
    [13, {bottomLeft: '=', topLeft: '+'}],
    [14, 'backspace'],

    [15, 'tab'],
    [16, {topLeft: 'q', bottomRight: ';', topRight: ':'}],
    [17, {topLeft: 'w', bottomRight: 'ς'}],
    [18, {topLeft: 'e', bottomRight: 'ε'}],
    [19, {topLeft: 'r', bottomRight: 'ρ'}],
    [20, {topLeft: 't', bottomRight: 'τ'}],
    [21, {topLeft: 'y', bottomRight: 'υ'}],
    [22, {topLeft: 'u', bottomRight: 'θ'}],
    [23, {topLeft: 'i', bottomRight: 'ι'}],
    [24, {topLeft: 'o', bottomRight: 'ο'}],
    [25, {topLeft: 'p', bottomRight: 'π'}],
    [26, {bottomLeft: '[', topLeft: '{'}],
    [27, {bottomLeft: ']', topLeft: '}'}],
    [43, {bottomLeft: '\\', topLeft: '|'}],

    [30, {topLeft: 'a', bottomRight: 'α'}],
    [31, {topLeft: 's', bottomRight: 'σ'}],
    [32, {topLeft: 'd', bottomRight: 'δ'}],
    [33, {topLeft: 'f', bottomRight: 'φ'}],
    [34, {topLeft: 'g', bottomRight: 'γ'}],
    [35, {topLeft: 'h', bottomRight: 'η'}],
    [36, {topLeft: 'j', bottomRight: 'ξ'}],
    [37, {topLeft: 'k', bottomRight: 'κ'}],
    [38, {topLeft: 'l', bottomRight: 'λ'}],
    [39, {bottomLeft: ';', topLeft: ':', topRight: '◌̈', bottomRight: '◌́'}],
    [40, {bottomLeft: '\'', topLeft: '"'}],
    [28, 'enter'],

    [42, 'shift'],
    [44, {topLeft: 'z', bottomRight: 'ζ'}],
    [45, {topLeft: 'x', bottomRight: 'χ'}],
    [46, {topLeft: 'c', bottomRight: 'ψ'}],
    [47, {topLeft: 'v', bottomRight: 'ω'}],
    [48, {topLeft: 'b', bottomRight: 'β'}],
    [49, {topLeft: 'n', bottomRight: 'ν'}],
    [50, {topLeft: 'm', bottomRight: 'μ'}],
    [51, {bottomLeft: ',', topLeft: '<'}],
    [52, {bottomLeft: '.', topLeft: '>'}],
    [53, {bottomLeft: '/', topLeft: '?'}],
    [54, 'shift'],

    [100, 'alt gr'],
  ],
  /* Hong Kong */
  'hk': kTraditionalChinese,
  /* Croatia */
  'hr': [
    ...kQwertzLetters,
    [41, {bottomLeft: '◌̧', topLeft: '◌̈'}],
    [2, {bottomLeft: '1', topLeft: '!', bottomRight: '~'}],
    [3, {bottomLeft: '2', topLeft: '"', bottomRight: '◌̌'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: '◌̂'}],
    [5, {bottomLeft: '4', topLeft: '$', bottomRight: '◌̆'}],
    [6, {bottomLeft: '5', topLeft: '%', bottomRight: '◌̊'}],
    [7, {bottomLeft: '6', topLeft: '&', bottomRight: '◌̢'}],
    [8, {bottomLeft: '7', topLeft: '/', bottomRight: '◌̀'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: '◌̇'}],
    [10, {bottomLeft: '9', topLeft: ')', bottomRight: '◌́'}],
    [11, {bottomLeft: '0', topLeft: '=', bottomRight: '◌̋'}],
    [12, {bottomLeft: '\'', topLeft: '?', bottomRight: '◌̈'}],
    [13, {bottomLeft: '+', topLeft: '*', bottomRight: '◌̧'}],

    [16, {main: 'q', bottomRight: '\\'}],
    [17, {main: 'w', bottomRight: '|'}],
    [18, {main: 'e', bottomRight: '€'}],
    [26, {main: 'š', bottomRight: '÷'}],
    [27, {main: 'đ', bottomRight: '×'}],

    [33, {main: 'f', bottomRight: '['}],
    [34, {main: 'g', bottomRight: ']'}],
    [37, {main: 'k', bottomRight: 'ł'}],
    [38, {main: 'l', bottomRight: 'Ł'}],
    [39, 'č'],
    [40, {main: 'ć', bottomRight: 'ß'}],
    [43, {main: 'ž', bottomRight: '¤'}],

    [86, {bottomLeft: '<', topLeft: '>'}],
    [47, {main: 'v', bottomRight: '@'}],
    [48, {main: 'b', bottomRight: '{'}],
    [49, {main: 'n', bottomRight: '}'}],
    [50, {main: 'm', bottomRight: '§'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],

    [100, 'alt gr'],
  ],
  /* Indonesia */
  'id': kUsEnglish,
  /* Ireland */
  'ie': kGbEnglish,
  /* Israel */
  'il': [
    [41, {bottomLeft: '`', topLeft: '~'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '@'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: '€'}],
    [5, {bottomLeft: '4', topLeft: '$', bottomRight: '₪'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '^'}],
    [8, {bottomLeft: '7', topLeft: '&'}],
    [9, {bottomLeft: '8', topLeft: '*'}],
    [10, {bottomLeft: '9', topLeft: ')'}],
    [11, {bottomLeft: '0', topLeft: '('}],
    [12, {bottomLeft: '-', topLeft: '_'}],
    [13, {bottomLeft: '=', topLeft: '+'}],

    [16, {bottomLeft: '/', topLeft: 'q'}],
    [17, {bottomLeft: '\'', topLeft: 'w'}],
    [18, {bottomLeft: 'ק', topLeft: 'e'}],
    [19, {bottomLeft: 'ר', topLeft: 'r'}],
    [20, {bottomLeft: 'א', topLeft: 't'}],
    [21, {bottomLeft: 'ט', topLeft: 'y'}],
    [22, {bottomLeft: 'ו', topLeft: 'u'}],
    [23, {bottomLeft: 'ן', topLeft: 'i'}],
    [24, {bottomLeft: 'ם', topLeft: 'o'}],
    [25, {bottomLeft: 'פ', topLeft: 'p'}],
    [26, {bottomLeft: '[', topLeft: '{'}],
    [27, {bottomLeft: ']', topLeft: '}'}],
    [43, {bottomLeft: '\\', topLeft: '|'}],

    [30, {bottomLeft: 'ש', topLeft: 'a'}],
    [31, {bottomLeft: 'ד', topLeft: 's'}],
    [32, {bottomLeft: 'ג', topLeft: 'd'}],
    [33, {bottomLeft: 'כ', topLeft: 'f'}],
    [34, {bottomLeft: 'ע', topLeft: 'g'}],
    [35, {bottomLeft: 'י', topLeft: 'h'}],
    [36, {bottomLeft: 'ח', topLeft: 'j'}],
    [37, {bottomLeft: 'ל', topLeft: 'k'}],
    [38, {bottomLeft: 'ך', topLeft: 'l'}],
    [39, {bottomLeft: 'ף', topLeft: ':'}],
    [40, {bottomLeft: ',', topLeft: '"'}],

    [44, {bottomLeft: 'ז', topLeft: 'z'}],
    [45, {bottomLeft: 'ס', topLeft: 'x'}],
    [46, {bottomLeft: 'ב', topLeft: 'c'}],
    [47, {bottomLeft: 'ה', topLeft: 'v'}],
    [48, {bottomLeft: 'נ', topLeft: 'b'}],
    [49, {bottomLeft: 'מ', topLeft: 'n'}],
    [50, {bottomLeft: 'צ', topLeft: 'm'}],
    [51, {bottomLeft: 'ת', topLeft: '<'}],
    [52, {bottomLeft: 'ץ', topLeft: '>'}],
    [53, {bottomLeft: '.', topLeft: '?'}],

    [100, 'alt gr'],
  ],
  /* India */
  'in': kUsEnglish,
  /* Iceland */
  'is': [
    ...kQwertyLetters,
    [41, {bottomLeft: '◌̊', topLeft: '◌̈'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '"'}],
    [4, {bottomLeft: '3', topLeft: '#'}],
    [5, {bottomLeft: '4', topLeft: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '/', bottomRight: '{'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: '['}],
    [10, {bottomLeft: '9', topLeft: ')', bottomRight: ']'}],
    [11, {bottomLeft: '0', topLeft: '=', bottomRight: '}'}],
    [12, {main: 'ö', bottomRight: '\\'}],
    [13, {bottomLeft: '-', topLeft: '_'}],

    [16, {main: 'q', bottomRight: '@'}],
    [18, {main: 'e', bottomRight: '€'}],
    [26, 'ð'],
    [27, {bottomLeft: '\'', topLeft: '?', bottomRight: '~'}],

    [39, 'æ'],
    [40, {bottomLeft: '◌́', topLeft: ' ', bottomRight: '◌̂'}],
    [43, {bottomLeft: '+', topLeft: '*', bottomRight: '◌̀'}],

    [86, {bottomLeft: '<', topLeft: '>', bottomRight: '|'}],
    [50, {main: 'm', bottomRight: 'µ'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, 'þ'],

    [100, 'alt gr'],
  ],
  /* Italy */
  'it': [
    ...kQwertyLetters,
    [41, {bottomLeft: '\\', topLeft: '|'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '"'}],
    [4, {bottomLeft: '3', topLeft: '£'}],
    [5, {bottomLeft: '4', topLeft: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '/'}],
    [9, {bottomLeft: '8', topLeft: '('}],
    [10, {bottomLeft: '9', topLeft: ')'}],
    [11, {bottomLeft: '0', topLeft: '='}],
    [12, {bottomLeft: '\'', topLeft: '?'}],
    [13, {bottomLeft: 'ì', topLeft: '◌̂'}],

    [18, {main: 'e', bottomRight: '€'}],
    [26, {bottomLeft: 'è', topLeft: 'é', bottomRight: '['}],
    [27, {bottomLeft: '+', topLeft: '*', bottomRight: ']'}],
    [28, 'invio'],

    [39, {bottomLeft: 'ò', topLeft: 'ç', bottomRight: '@'}],
    [40, {bottomLeft: 'à', topLeft: '°', bottomRight: '#'}],
    [43, {bottomLeft: 'ù', topLeft: '§'}],

    [42, 'maiusc'],
    [86, {bottomLeft: '<', topLeft: '>'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],
    [54, 'maiusc'],

    [100, 'alt gr'],
  ],
  /* Japan */
  'jp': [
    [
      41,
      {
        icon: 'keyboard:jis-letter-switch',
        ariaNameI18n: 'keyboardDiagramAriaNameJisLetterSwitch',
      },
    ],
    [2, {bottomLeft: '1', topLeft: '!', bottomRight: 'ぬ'}],
    [3, {bottomLeft: '2', topLeft: '"', bottomRight: 'ふ'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: 'あ'}],
    [5, {bottomLeft: '4', topLeft: '$', bottomRight: 'う'}],
    [6, {bottomLeft: '5', topLeft: '%', bottomRight: 'え'}],
    [7, {bottomLeft: '6', topLeft: '&', bottomRight: 'お'}],
    [8, {bottomLeft: '7', topLeft: '\'', bottomRight: 'や'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: 'ゆ'}],
    [10, {bottomLeft: '9', topLeft: ')', bottomRight: 'よ'}],
    [11, {bottomLeft: '0', topLeft: ' ', topRight: 'を', bottomRight: 'わ'}],
    [12, {bottomLeft: '-', topLeft: '=', bottomRight: 'ほ'}],
    [13, {bottomLeft: '^', topLeft: '~', bottomRight: 'へ'}],
    [124, {bottomLeft: '¥', topLeft: '|', bottomRight: 'ー'}],

    [16, {topLeft: 'q', bottomRight: 'た'}],
    [17, {topLeft: 'w', bottomRight: 'て'}],
    [18, {topLeft: 'e', bottomRight: 'い'}],
    [19, {topLeft: 'r', bottomRight: 'す'}],
    [20, {topLeft: 't', bottomRight: 'か'}],
    [21, {topLeft: 'y', bottomRight: 'ん'}],
    [22, {topLeft: 'u', bottomRight: 'な'}],
    [23, {topLeft: 'i', bottomRight: 'に'}],
    [24, {topLeft: 'o', bottomRight: 'ら'}],
    [25, {topLeft: 'p', bottomRight: 'せ'}],
    [26, {bottomLeft: '@', topLeft: '`', bottomRight: '゛'}],
    [27, {bottomLeft: '[', topLeft: '{', bottomRight: '゜'}],

    [30, {topLeft: 'a', bottomRight: 'ち'}],
    [31, {topLeft: 's', bottomRight: 'と'}],
    [32, {topLeft: 'd', bottomRight: 'し'}],
    [33, {topLeft: 'f', bottomRight: 'は'}],
    [34, {topLeft: 'g', bottomRight: 'き'}],
    [35, {topLeft: 'h', bottomRight: 'く'}],
    [36, {topLeft: 'j', bottomRight: 'ま'}],
    [37, {topLeft: 'k', bottomRight: 'の'}],
    [38, {topLeft: 'l', bottomRight: 'り'}],
    [39, {bottomLeft: ';', topLeft: '+', bottomRight: 'れ'}],
    [40, {bottomLeft: ':', topLeft: '*', bottomRight: 'け'}],
    [43, {bottomLeft: ']', topLeft: '}', bottomRight: 'む'}],

    [44, {topLeft: 'z', bottomRight: 'つ'}],
    [45, {topLeft: 'x', bottomRight: 'さ'}],
    [46, {topLeft: 'c', bottomRight: 'そ'}],
    [47, {topLeft: 'v', bottomRight: 'ひ'}],
    [48, {topLeft: 'b', bottomRight: 'こ'}],
    [49, {topLeft: 'n', bottomRight: 'み'}],
    [50, {topLeft: 'm', bottomRight: 'も'}],
    [51, {bottomLeft: ',', topLeft: '<', topRight: '､', bottomRight: 'ね'}],
    [52, {bottomLeft: '.', topLeft: '>', topRight: '。', bottomRight: 'る'}],
    [53, {bottomLeft: '/', topLeft: '?', topRight: '•', bottomRight: 'め'}],
    [89, {bottomLeft: '\\', topLeft: '_', bottomRight: 'ろ'}],
  ],
  /* Japan with US keyboard */
  'jp.us': kUsEnglish,
  /* South Korea */
  'kr': [
    [41, {bottomLeft: '◌̀', topLeft: '~'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '@'}],
    [4, {bottomLeft: '3', topLeft: '#'}],
    [5, {bottomLeft: '4', topLeft: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '^'}],
    [8, {bottomLeft: '7', topLeft: '&'}],
    [9, {bottomLeft: '8', topLeft: '*'}],
    [10, {bottomLeft: '9', topLeft: '('}],
    [11, {bottomLeft: '0', topLeft: ')'}],
    [12, {bottomLeft: '-', topLeft: '_'}],
    [13, {bottomLeft: '=', topLeft: '+'}],

    [16, {topLeft: 'q', bottomRight: 'ㅂ', topRight: 'ㅃ'}],
    [17, {topLeft: 'w', bottomRight: 'ㅈ', topRight: 'ㅉ'}],
    [18, {topLeft: 'e', bottomRight: 'ㄷ', topRight: 'ㄸ'}],
    [19, {topLeft: 'r', bottomRight: 'ㄱ', topRight: 'ㄲ'}],
    [20, {topLeft: 't', bottomRight: 'ㅅ', topRight: 'ㅆ'}],
    [21, {topLeft: 'y', bottomRight: 'ㅛ'}],
    [22, {topLeft: 'u', bottomRight: 'ㅕ'}],
    [23, {topLeft: 'i', bottomRight: 'ㅑ'}],
    [24, {topLeft: 'o', bottomRight: 'ㅐ', topRight: 'ㅒ'}],
    [25, {topLeft: 'p', bottomRight: 'ㅔ', topRight: 'ㅖ'}],
    [26, {bottomLeft: '[', topLeft: '{'}],
    [27, {bottomLeft: ']', topLeft: '}'}],
    [43, {bottomLeft: '\\', topLeft: '|', bottomRight: '₩'}],

    [30, {topLeft: 'a', bottomRight: 'ㅁ'}],
    [31, {topLeft: 's', bottomRight: 'ㄴ'}],
    [32, {topLeft: 'd', bottomRight: 'ㅇ'}],
    [33, {topLeft: 'f', bottomRight: 'ㄹ'}],
    [34, {topLeft: 'g', bottomRight: 'ㅎ'}],
    [35, {topLeft: 'h', bottomRight: 'ㅗ'}],
    [36, {topLeft: 'j', bottomRight: 'ㅓ'}],
    [37, {topLeft: 'k', bottomRight: 'ㅏ'}],
    [38, {topLeft: 'l', bottomRight: 'ㅣ'}],
    [39, {bottomLeft: ';', topLeft: ':'}],
    [40, {bottomLeft: '\'', topLeft: '"'}],

    [44, {topLeft: 'z', bottomRight: 'ㅋ'}],
    [45, {topLeft: 'x', bottomRight: 'ㅌ'}],
    [46, {topLeft: 'c', bottomRight: 'ㅊ'}],
    [47, {topLeft: 'v', bottomRight: 'ㅍ'}],
    [48, {topLeft: 'b', bottomRight: 'ㅠ'}],
    [49, {topLeft: 'n', bottomRight: 'ㅜ'}],
    [50, {topLeft: 'm', bottomRight: 'ㅡ'}],
    [51, {bottomLeft: ',', topLeft: '<'}],
    [52, {bottomLeft: '.', topLeft: '>'}],
    [53, {bottomLeft: '/', topLeft: '?'}],

    // TODO(b/221928190): find a way to distinguish between the "compact" and
    // "normal" variants, rather than assuming that the compact one is in use.
    [100, '한/영'],
    [97, '한자'],
  ],
  /* Kuwait */
  'kw': kArabic,
  /* Kazakhstan */
  'kz': kUsEnglish,
  /* Hispanophone Latin America */
  'latam-es-419': kSpainSpanish,
  /* Mexico */
  'mx': kLatamSpanish,
  /* Malaysia */
  'my': kUsEnglish,
  /* Nigeria */
  'ng': kUsEnglishInternational,
  /* Netherlands */
  'nl': kUsEnglishInternational,
  /* Norway */
  'no': kNordic,
  /* Nordics */
  'nordic': kNordic,
  /* New Zealand */
  'nz': kUsEnglish,
  /* Oman */
  'om': kArabic,
  /* Peru */
  'pe': kLatamSpanish,
  /* Philippines */
  'ph': kUsEnglish,
  /* Poland */
  'pl': [
    /*
     * Depending on the variant, this layout might have symbols or text labels
     * on Tab, Shift, and Backspace. Since there is no way for code to
     * distinguish between them, err on the side of symbols.
     */
    ...kUsEnglishNoSideLabels,
    [100, 'alt gr'],
  ],
  /* Portugal */
  'pt': [
    ...kQwertyLetters,
    [41, {bottomLeft: '\\', topLeft: '|'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '"', bottomRight: '@'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: '£'}],
    [5, {bottomLeft: '4', topLeft: '$', bottomRight: '§'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '/', bottomRight: '{'}],
    [9, {bottomLeft: '8', topLeft: '(', bottomRight: '['}],
    [10, {bottomLeft: '9', topLeft: ')', bottomRight: ']'}],
    [11, {bottomLeft: '0', topLeft: '=', bottomRight: '}'}],
    [12, {bottomLeft: '\'', topLeft: '?'}],
    [13, {bottomLeft: '«', topLeft: '»'}],

    [26, {bottomLeft: '+', topLeft: '*', bottomRight: '~'}],
    [27, {bottomLeft: '◌́', topLeft: '◌̀'}],

    [39, 'ç'],
    [40, {bottomLeft: 'º', topLeft: 'ª'}],
    [43, {bottomLeft: '~', topLeft: '◌̂'}],

    [86, {bottomLeft: '<', topLeft: '>'}],
    [51, {bottomLeft: ',', topLeft: ';'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],

    [100, 'alt gr'],
  ],
  /* Qatar */
  'qa': kArabic,
  /* Romania */
  'ro': kRoRomanian,
  /* Romania with US keyboard */
  'ro.us': kUsEnglish,
  /* Russia */
  'ru': [
    [41, {bottomLeft: '`', topLeft: '~', bottomRight: 'ë'}],
    [2, {bottomLeft: '1', topLeft: '!', topRight: ' '}],
    [3, {bottomLeft: '2', topLeft: '@', topRight: '"'}],
    [4, {bottomLeft: '3', topLeft: '#', topRight: '№'}],
    [5, {bottomLeft: '4', topLeft: '$', topRight: ';'}],
    [6, {bottomLeft: '5', topLeft: '%', topRight: ' '}],
    [7, {bottomLeft: '6', topLeft: '◌̂', topRight: ':'}],
    [8, {bottomLeft: '7', topLeft: '&', topRight: '?'}],
    [9, {bottomLeft: '8', topLeft: '*', topRight: ' '}],
    [10, {bottomLeft: '9', topLeft: '(', topRight: ' '}],
    [11, {bottomLeft: '0', topLeft: ')', topRight: ' '}],
    [12, {bottomLeft: '-', topLeft: '_', topRight: ' '}],
    [13, {bottomLeft: '=', topLeft: '+', topRight: ' '}],

    [16, {topLeft: 'q', bottomRight: 'й'}],
    [17, {topLeft: 'w', bottomRight: 'ц'}],
    [18, {topLeft: 'e', bottomRight: 'у'}],
    [19, {topLeft: 'r', bottomRight: 'к'}],
    [20, {topLeft: 't', bottomRight: 'е'}],
    [21, {topLeft: 'y', bottomRight: 'н'}],
    [22, {topLeft: 'u', bottomRight: 'г'}],
    [23, {topLeft: 'i', bottomRight: 'ш'}],
    [24, {topLeft: 'o', bottomRight: 'щ'}],
    [25, {topLeft: 'p', bottomRight: 'з'}],
    [26, {bottomLeft: '[', topLeft: '{', bottomRight: 'х'}],
    [27, {bottomLeft: ']', topLeft: '}', bottomRight: 'ъ'}],
    [43, {bottomLeft: '\\', topLeft: '|', topRight: '/'}],

    [30, {topLeft: 'a', bottomRight: 'ф'}],
    [31, {topLeft: 's', bottomRight: 'ы'}],
    [32, {topLeft: 'd', bottomRight: 'в'}],
    [33, {topLeft: 'f', bottomRight: 'а'}],
    [34, {topLeft: 'g', bottomRight: 'п'}],
    [35, {topLeft: 'h', bottomRight: 'р'}],
    [36, {topLeft: 'j', bottomRight: 'о'}],
    [37, {topLeft: 'k', bottomRight: 'л'}],
    [38, {topLeft: 'l', bottomRight: 'д'}],
    [39, {bottomLeft: ';', topLeft: ':', bottomRight: 'ж'}],
    [40, {bottomLeft: '\'', topLeft: '"', bottomRight: 'э'}],

    [44, {topLeft: 'z', bottomRight: 'я'}],
    [45, {topLeft: 'x', bottomRight: 'ч'}],
    [46, {topLeft: 'c', bottomRight: 'с'}],
    [47, {topLeft: 'v', bottomRight: 'м'}],
    [48, {topLeft: 'b', bottomRight: 'и'}],
    [49, {topLeft: 'n', bottomRight: 'т'}],
    [50, {topLeft: 'm', bottomRight: 'ь'}],
    [51, {bottomLeft: ',', topLeft: '<', bottomRight: 'б'}],
    [52, {bottomLeft: '.', topLeft: '>', bottomRight: 'ю'}],
    [53, {bottomLeft: '/', topLeft: '?', bottomRight: '.', topRight: ','}],
  ],
  /* Saudi Arabia */
  'sa': kUsEnglish,
  /* Sweden */
  'se': kNordic,
  /* Singapore */
  'sg': kUsEnglish,
  /* Slovakia */
  'sk': [
    ...kQwertzLetters,
    [41, {bottomLeft: ';', topLeft: '°'}],
    [2, {bottomLeft: '+', topLeft: '1'}],
    [3, {bottomLeft: 'ľ', topLeft: '2'}],
    [4, {bottomLeft: 'š', topLeft: '3'}],
    [5, {bottomLeft: 'č', topLeft: '4'}],
    [6, {bottomLeft: 'ť', topLeft: '5'}],
    [7, {bottomLeft: 'ž', topLeft: '6'}],
    [8, {bottomLeft: 'ý', topLeft: '7'}],
    [9, {bottomLeft: 'á', topLeft: '8'}],
    [10, {bottomLeft: 'í', topLeft: '9'}],
    [11, {bottomLeft: 'é', topLeft: '0'}],
    [12, {bottomLeft: '=', topLeft: '%'}],
    [13, {bottomLeft: '◌́', topLeft: '◌̌'}],

    [18, {main: 'e', bottomRight: '€'}],
    [26, {bottomLeft: 'ú', topLeft: '/'}],
    [27, {bottomLeft: 'ä', topLeft: '('}],

    [39, {bottomLeft: 'ô', topLeft: '"'}],
    [40, {bottomLeft: '§', topLeft: '!'}],
    [43, {bottomLeft: 'ň', topLeft: '!'}],

    [86, {bottomLeft: '&', topLeft: '*'}],
    [51, {bottomLeft: ',', topLeft: '?'}],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: '-', topLeft: '_'}],

    [100, 'alt gr'],
  ],
  /* Thailand */
  'th': [
    [41, {bottomLeft: '◌̀', topLeft: '~', bottomRight: '_', topRight: '%'}],
    [2, {bottomLeft: '1', topLeft: '!', bottomRight: 'ๅ', topRight: '+'}],
    [3, {bottomLeft: '2', topLeft: '@', bottomRight: '/', topRight: '๑'}],
    [4, {bottomLeft: '3', topLeft: '#', bottomRight: '-', topRight: '๒'}],
    [5, {bottomLeft: '4', topLeft: '$', bottomRight: 'ภ', topRight: '๓'}],
    [6, {bottomLeft: '5', topLeft: '%', bottomRight: 'ถ', topRight: '๔'}],
    [7, {bottomLeft: '6', topLeft: '◌̂', bottomRight: '◌ุ', topRight: '◌ู'}],
    [8, {bottomLeft: '7', topLeft: '&', bottomRight: '◌ึ', topRight: '฿'}],
    [9, {bottomLeft: '8', topLeft: '*', bottomRight: 'ค', topRight: '๕'}],
    [10, {bottomLeft: '9', topLeft: '(', bottomRight: 'ต', topRight: '๖'}],
    [11, {bottomLeft: '0', topLeft: ')', bottomRight: 'จ', topRight: '๗'}],
    [12, {bottomLeft: '-', topLeft: '_', bottomRight: 'ข', topRight: '๘'}],
    [13, {bottomLeft: '=', topLeft: '+', bottomRight: 'ช', topRight: '๙'}],

    [16, {bottomLeft: 'q', bottomRight: 'ๆ', topRight: '๐'}],
    [17, {bottomLeft: 'w', bottomRight: 'ไ', topRight: '"'}],
    [18, {bottomLeft: 'e', bottomRight: '◌ำ', topRight: 'ฎ'}],
    [19, {bottomLeft: 'r', bottomRight: 'พ', topRight: 'ฑ'}],
    [20, {bottomLeft: 't', bottomRight: 'ะ', topRight: 'ธ'}],
    [21, {bottomLeft: 'y', bottomRight: '◌ั', topRight: '◌ํ'}],
    [22, {bottomLeft: 'u', bottomRight: '◌ี', topRight: '◌๊'}],
    [23, {bottomLeft: 'i', bottomRight: 'ร', topRight: 'ณ'}],
    [24, {bottomLeft: 'o', bottomRight: 'น', topRight: 'ฯ'}],
    [25, {bottomLeft: 'p', bottomRight: 'ย', topRight: 'ญ'}],
    [26, {bottomLeft: '[', topLeft: '{', bottomRight: 'บ', topRight: 'ฐ'}],
    [27, {bottomLeft: ']', topLeft: '}', bottomRight: 'ล', topRight: ','}],
    [43, {bottomLeft: '\\', topLeft: '|', bottomRight: 'ฃ', topRight: 'ฅ'}],

    [30, {bottomLeft: 'a', bottomRight: 'ฟ', topRight: 'ฤ'}],
    [31, {bottomLeft: 's', bottomRight: 'ห', topRight: 'ฆ'}],
    [32, {bottomLeft: 'd', bottomRight: 'ก', topRight: 'ฏ'}],
    [33, {bottomLeft: 'f', bottomRight: 'ด', topRight: 'โ'}],
    [34, {bottomLeft: 'g', bottomRight: 'เ', topRight: 'ฌ'}],
    [35, {bottomLeft: 'h', bottomRight: '◌้', topRight: '◌็'}],
    [36, {bottomLeft: 'j', bottomRight: '◌่', topRight: '◌๋'}],
    [37, {bottomLeft: 'k', bottomRight: 'า', topRight: 'ษ'}],
    [38, {bottomLeft: 'l', bottomRight: 'ส', topRight: 'ศ'}],
    [39, {bottomLeft: ';', topLeft: ':', bottomRight: 'ว', topRight: 'ซ'}],
    [40, {bottomLeft: '\'', topLeft: '"', bottomRight: 'ง', topRight: '.'}],

    [44, {bottomLeft: 'z', bottomRight: 'ผ', topRight: '('}],
    [45, {bottomLeft: 'x', bottomRight: 'ป', topRight: ')'}],
    [46, {bottomLeft: 'c', bottomRight: 'แ', topRight: 'ฉ'}],
    [47, {bottomLeft: 'v', bottomRight: 'อ', topRight: 'ฮ'}],
    [48, {bottomLeft: 'b', bottomRight: '◌ิ', topRight: '◌ฺ'}],
    [49, {bottomLeft: 'n', bottomRight: '◌ื', topRight: '◌์'}],
    [50, {bottomLeft: 'm', bottomRight: 'ท', topRight: '?'}],
    [51, {bottomLeft: ',', topLeft: '<', bottomRight: 'ม', topRight: 'ฒ'}],
    [52, {bottomLeft: '.', topLeft: '>', bottomRight: 'ใ', topRight: 'ฬ'}],
    [53, {bottomLeft: '/', topLeft: '?', bottomRight: 'ฝ', topRight: 'ฦ'}],
  ],
  /* Turkey */
  'tr': [
    ...kQwertyLetters,
    [41, {bottomLeft: '"', topLeft: 'é'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '\''}],
    [4, {bottomLeft: '3', topLeft: '◌̂'}],
    [5, {bottomLeft: '4', topLeft: '+'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '/'}],
    [9, {bottomLeft: '8', topLeft: '('}],
    [10, {bottomLeft: '9', topLeft: ')'}],
    [11, {bottomLeft: '0', topLeft: '='}],
    [12, {bottomLeft: '*', topLeft: '?'}],
    [13, {bottomLeft: '-', topLeft: '_'}],

    [16, {main: 'q', bottomRight: '@'}],
    [18, {main: 'e', bottomRight: '€'}],
    [20, {main: 't', bottomRight: '₺'}],
    [23, 'ı'],
    [26, 'ğ'],
    [27, 'ü'],

    [39, 'ş'],
    [40, 'i'],
    [43, {bottomLeft: ',', topLeft: ';'}],

    [51, 'ö'],
    [52, 'ç'],
    [53, {bottomLeft: '.', topLeft: ':'}],

    [86, {bottomLeft: '<', topLeft: '>'}],

    [100, 'alt gr'],
  ],
  'tr.f': [
    [41, {bottomLeft: '+', topLeft: '*'}],
    [2, {bottomLeft: '1', topLeft: '!'}],
    [3, {bottomLeft: '2', topLeft: '"'}],
    [4, {bottomLeft: '3', topLeft: '◌̂'}],
    [5, {bottomLeft: '4', topLeft: '$'}],
    [6, {bottomLeft: '5', topLeft: '%'}],
    [7, {bottomLeft: '6', topLeft: '&'}],
    [8, {bottomLeft: '7', topLeft: '\''}],
    [9, {bottomLeft: '8', topLeft: '('}],
    [10, {bottomLeft: '9', topLeft: ')'}],
    [11, {bottomLeft: '0', topLeft: '='}],
    [12, {bottomLeft: '/', topLeft: '?'}],
    [13, {bottomLeft: '-', topLeft: '_'}],

    [16, 'f'],
    [17, 'g'],
    [18, 'ğ'],
    [19, 'ı'],
    [20, 'o'],
    [21, 'd'],
    [22, 'r'],
    [23, 'n'],
    [24, 'h'],
    [25, 'p'],
    [26, 'q'],
    [27, 'w'],

    [30, 'u'],
    [31, 'i'],
    [32, 'e'],
    [33, 'a'],
    [34, 'ü'],
    [35, 't'],
    [36, 'k'],
    [37, 'm'],
    [38, 'l'],
    [39, 'y'],
    [40, 'ş'],
    [43, 'x'],

    [44, 'j'],
    [45, 'ö'],
    [46, 'v'],
    [47, 'c'],
    [48, 'ç'],
    [49, 'z'],
    [50, 's'],
    [51, 'b'],
    [52, {bottomLeft: '.', topLeft: ':'}],
    [53, {bottomLeft: ',', topLeft: ';'}],

    [86, {bottomLeft: '<', topLeft: '>'}],

    [100, 'alt gr'],
  ],
  /* Taiwan */
  'tw': kTraditionalChinese,
  /* United States */
  'us': kUsEnglish,
  /* United States (English Intl) */
  'us.intl': kUsEnglishInternational,
  /* Uruguay */
  'uy': kLatamSpanish,
  /* Vietnam */
  'vn': kUsEnglish,
  /* South Africa */
  'za': kGbEnglish,
  /* South Africa */
  'za.us': kUsEnglish,
};

export function getKeyboardLayoutForRegionCode(regionCode) {
  if (regionCode in kLayouts) {
    return new Map(kLayouts[regionCode]);
  } else {
    console.warn('No visual layout for region code ' + regionCode);
    return null;
  }
}
