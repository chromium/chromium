// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

DictationLocaleInfoTest = class extends DictationE2ETestBase {};

AX_TEST_F('DictationLocaleInfoTest', 'AllowSmartCapAndSpacing', function() {
  // Restrict behavior to English + FIGS (French, Italian, German, Spanish).
  LocaleInfo.locale = 'en-US';
  assertTrue(LocaleInfo.allowSmartCapAndSpacing());
  LocaleInfo.locale = 'fr';
  assertTrue(LocaleInfo.allowSmartCapAndSpacing());
  LocaleInfo.locale = 'it-IT';
  assertTrue(LocaleInfo.allowSmartCapAndSpacing());
  LocaleInfo.locale = 'de';
  assertTrue(LocaleInfo.allowSmartCapAndSpacing());
  LocaleInfo.locale = 'es';
  assertTrue(LocaleInfo.allowSmartCapAndSpacing());

  LocaleInfo.locale = 'ja-JP';
  assertFalse(LocaleInfo.allowSmartCapAndSpacing());
});

AX_TEST_F('DictationLocaleInfoTest', 'AllowSmartEditing', function() {
  // Restrict behavior to left-to-right locales.
  LocaleInfo.locale = 'en-US';
  assertTrue(LocaleInfo.allowSmartEditing());
  LocaleInfo.locale = 'ja-JP';
  assertTrue(LocaleInfo.allowSmartEditing());

  LocaleInfo.locale = 'ar-LB';
  assertFalse(LocaleInfo.allowSmartEditing());
});

AX_TEST_F('DictationLocaleInfoTest', 'IsRTLLocale', function() {
  LocaleInfo.locale = 'ja-JP';
  assertFalse(LocaleInfo.isRTLLocale());
  LocaleInfo.locale = 'ar-LB';
  assertTrue(LocaleInfo.isRTLLocale());
});

AX_TEST_F('DictationLocaleInfoTest', 'GetUILanguage', function() {
  LocaleInfo.locale = 'iw-il';
  assertEquals('he', LocaleInfo.getUILanguage());
  LocaleInfo.locale = 'iw-IL';
  assertEquals('he', LocaleInfo.getUILanguage());
  LocaleInfo.locale = 'yue-hant-hk';
  assertEquals('zh-tw', LocaleInfo.getUILanguage());
  LocaleInfo.locale = 'no-no';
  assertEquals('nb', LocaleInfo.getUILanguage());
  LocaleInfo.locale = 'en-US';
  assertEquals(undefined, LocaleInfo.getUILanguage());
});

AX_TEST_F('DictationLocaleInfoTest', 'AreCommandsSupported', function() {
  let systemLocale;
  chrome.i18n.getUILanguage = () => {
    return systemLocale;
  };
  const areCommandsSupported = LocaleInfo.areCommandsSupported;

  // True if the language part of the code matches.
  LocaleInfo.locale = 'en-US';
  systemLocale = 'en';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'EN-US';
  systemLocale = 'en';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'en-US';
  systemLocale = 'en-GB';
  assertTrue(areCommandsSupported());

  // False if the language part of the code doesn't match, in most cases.
  LocaleInfo.locale = 'en-US';
  systemLocale = 'ja-JP';
  assertFalse(areCommandsSupported());

  LocaleInfo.locale = 'ja-JP';
  systemLocale = 'en-US';
  assertFalse(areCommandsSupported());

  // Special cases: these Dictation locales can map to UI languages.
  LocaleInfo.locale = 'iw-IL';
  systemLocale = 'he';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'iw-IL';
  systemLocale = 'he-IL';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'no-NO';
  systemLocale = 'nb';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'no-NO';
  systemLocale = 'nb-NB';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'yue-Hant-HK';
  systemLocale = 'zh-TW';
  assertTrue(areCommandsSupported());

  LocaleInfo.locale = 'yue-Hant-HK';
  systemLocale = 'zh';
  assertFalse(areCommandsSupported());

  LocaleInfo.locale = 'yue-Hant-HK';
  systemLocale = 'zh-CN';
  assertFalse(areCommandsSupported());
});
