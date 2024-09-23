// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for LocaleOutputHelper.
 */
ChromeVoxLocaleOutputHelperTest = class extends ChromeVoxE2ETest {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "base/command_line.h"
  #include "ui/accessibility/accessibility_switches.h"
  #include "ui/base/ui_base_switches.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(::switches::kLang, "en-US");
      `);
    super.testGenPreamble();
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Mock this api to return a predefined set of voices.
    chrome.tts.getVoices = function(callback) {
      callback([
        // All properties of TtsVoice object are optional.
        // https://developer.chrome.com/apps/tts#type-TtsVoice.
        {},
        {voiceName: 'Android'},
        {'lang': 'en-US'},
        {'lang': 'fr-CA'},
        {'lang': 'es-ES'},
        {'lang': 'it-IT'},
        {'lang': 'ja-JP'},
        {'lang': 'ko-KR'},
        {'lang': 'zh-TW'},
        {'lang': 'ast'},
        {'lang': 'pt'},
      ]);
    };
  }

  /**
   * Calls mock version of chrome.tts.getVoices() to populate
   * LocaleOutputHelper's available voice list with a specific set of voices.
   */
  setAvailableVoices() {
    chrome.tts.getVoices(function(voices) {
      LocaleOutputHelper.instance.availableVoices_ = voices;
    });
  }

  get asturianAndJapaneseDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ja">ど</p>
      <p lang="ast">
        Pretend that this text is Asturian. Testing three-letter language code logic.
      </p>
    `;
  }

  get buttonAndLinkDoc() {
    return `
      <body lang="es">
        <p>This is a paragraph, written in English.</p>
        <button type="submit">This is a button, written in English.</button>
        <a href="https://www.google.com">Este es un enlace.</a>
      </body>
    `;
  }

  get englishAndFrenchUnlabeledDoc() {
    return `
      <p>
        This entire object should be read in English, even the following French passage:
        salut mon ami! Ca va? Bien, et toi? It's hard to differentiate between latin-based languages.
      </p>
    `;
  }

  get englishAndKoreanUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>This text is written in English. 차에 한하여 중임할 수. This text is also written in English.</p>
    `;
  }

  get japaneseAndChineseUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p id="text">
        天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追
      </p>
    `;
  }

  get japaneseAndEnglishUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>Hello, my name is 太田あきひろ. It's a pleasure to meet you. どうぞよろしくお願いします.</p>
    `;
  }

  get japaneseAndKoreanUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ko">
        私は. 법률이 정하는 바에 의하여 대법관이 아닌 법관을 둘 수 있다
      </p>
    `;
  }

  get japaneseCharacterUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>ど</p>
    `;
  }

  get multipleLanguagesLabeledDoc() {
    return `
      <p lang="es">Hola.</p>
      <p lang="en">Hello.</p>
      <p lang="fr">Salut.</p>
      <span lang="it">Ciao amico.</span>
    `;
  }

  get japaneseAndInvalidLanguagesLabeledDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ja">どうぞよろしくお願いします</p>
      <p lang="invalid-code">Test</p>
      <p lang="hello">Yikes</p>
    `;
  }

  get nestedLanguagesLabeledDoc() {
    return `
      <p id="breakfast" lang="en">In the morning, I sometimes eat breakfast.</p>
      <p id="lunch" lang="fr">Dans l'apres-midi, je dejeune.</p>
      <p id="greeting" lang="en">
        Hello it's a pleasure to meet you.
        <span lang="fr">Comment ca va?</span>Switching back to English.
        <span lang="es">Hola.</span>Goodbye.
      </p>
    `;
  }

  get vietnameseAndUrduLabeledDoc() {
    return `
      <p lang="vi">Vietnamese text.</p>
      <p lang="ur">Urdu text.</p>
    `;
  }

  get chineseDoc() {
    return `
      <p lang="en-us">United States</p>
      <p lang="zh-hans">Simplified Chinese</p>
      <p lang="zh-hant">Traditional Chinese</p>
    `;
  }

  get portugueseDoc() {
    return `
      <p lang="en-us">United States</p>
      <p lang="pt-br">Brazil</p>
      <p lang="pt-pt">Portugal</p>
    `;
  }
};

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'MultipleLanguagesLabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.multipleLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('es', 'español: Hola.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('en', 'English: Hello.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('fr', 'français: Salut.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('it', 'italiano: Ciao amico.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'NestedLanguagesLabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.nestedLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en', 'In the morning, I sometimes eat breakfast.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'fr', 'français: Dans l\'apres-midi, je dejeune.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'en', 'English: Hello it\'s a pleasure to meet you. ');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('fr', 'français: Comment ca va?');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('en', 'English: Switching back to English. ');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('es', 'español: Hola.');
      mockFeedback.call(doCmd('nextLine'))
          .expectSpeechWithLocale('en', 'English: Goodbye.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'ButtonAndLinkDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.buttonAndLinkDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback
          .call(doCmd('jumpToTop'))
          // Use the author-provided language of 'es'.
          .expectSpeechWithLocale(
              'es', 'español: This is a paragraph, written in English.')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('es', 'This is a button, written in English.')
          .expectSpeechWithLocale(
              undefined, 'Button', 'Press Search+Space to activate')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('es', 'Este es un enlace.')
          .expectSpeechWithLocale(undefined, 'Link');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndEnglishUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.japaneseAndEnglishUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback
          .call(doCmd('jumpToTop'))
          // Expect the node's contents to be read in one language
          // (English).
          // Language detection does not run on small runs of text, like
          // the one in this test, we are falling back on the UI language
          // of the browser, which is en-US. Please see testGenPreamble
          // for more details.
          .expectSpeechWithLocale(
              'en-us',
              'Hello, my name is 太田あきひろ. It\'s a pleasure to meet' +
                  ' you. どうぞよろしくお願いします.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'EnglishAndKoreanUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.englishAndKoreanUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en-us',
              'This text is written in English. 차에 한하여 중임할 수.' +
                  ' This text is also written in English.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'EnglishAndFrenchUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.englishAndFrenchUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en',
              'This entire object should be read in English, even' +
                  ' the following French passage: ' +
                  'salut mon ami! Ca va? Bien, et toi? It\'s hard to' +
                  ' differentiate between latin-based languages.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseCharacterUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.japaneseCharacterUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('en-us', 'ど');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndChineseUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.japaneseAndChineseUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en-us',
              '天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndChineseLabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      // Only difference between doc used in this test and
      // this.japaneseAndChineseUnlabeledDoc is the lang="zh" attribute.
      const root = await this.runWithLoadedTree(`
        <meta charset="utf-8">
        <p lang="zh">
          天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追
        </p>
    `);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'zh',
              '中文: 天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndKoreanUnlabeledDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.japaneseAndKoreanUnlabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      // Language detection runs and assigns language of 'ko' to the node.
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'ko',
              '한국어: 私は. 법률이 정하는 바에 의하여 대법관이 아닌 법관을 둘 수' +
                  ' 있다');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'AsturianAndJapaneseDocTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.asturianAndJapaneseDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('ja', '日本語: ど')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale(
              'ast',
              'asturianu: Pretend that this text is Asturian. Testing' +
                  ' three-letter language code logic.');
      await mockFeedback.replay();
    });


AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'LanguageSwitchingOffTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.multipleLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', false);
      this.setAvailableVoices();
      // Locale should not be set if the language switching feature is off.
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(undefined, 'Hola.')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale(undefined, 'Hello.')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale(undefined, 'Salut.')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale(undefined, 'Ciao amico.');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'DefaultToUILocaleTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(
          this.japaneseAndInvalidLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('ja', '日本語: どうぞよろしくお願いします')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('en-us', 'English (United States): Test')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('en-us', 'Yikes');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'NoAvailableVoicesTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root =
          await this.runWithLoadedTree(this.vietnameseAndUrduLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en-us', 'No voice available for language: Vietnamese')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale(
              'en-us', 'No voice available for language: Urdu');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'WordNavigationTest', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.nestedLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en', 'In the morning, I sometimes eat breakfast.')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'fr', 'français: Dans l\'apres-midi, je dejeune.')
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `l'apres`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `-`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `midi`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `,`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `je`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `dejeune`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `.`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `English: Hello`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `it's`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `a`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `pleasure`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `to`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `meet`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `you`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('en', `.`)
          .call(doCmd('nextWord'))
          .expectSpeechWithLocale('fr', `français: Comment`)
          .call(doCmd('previousWord'))
          .expectSpeechWithLocale('en', `English: .`)
          .call(doCmd('previousWord'))
          .expectSpeechWithLocale('en', `you`);
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'CharacterNavigationTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.nestedLanguagesLabeledDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale(
              'en', 'In the morning, I sometimes eat breakfast.')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'fr', 'français: Dans l\'apres-midi, je dejeune.')
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('fr', `a`)
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('fr', `n`)
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('fr', `s`)
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('fr', ` `)
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('fr', `l`)
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'en', `English: Hello it's a pleasure to meet you. `)
          .call(doCmd('nextCharacter'))
          .expectSpeechWithLocale('en', `e`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('en', `H`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `français: .`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `e`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `n`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `u`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `e`)
          .call(doCmd('previousCharacter'))
          .expectSpeechWithLocale('fr', `j`);
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'SwitchBetweenChineseDialectsTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.chineseDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('en-us', 'United States')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale('zh-hans', '中文（简体）: Simplified Chinese')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale(
              'zh-hant', '中文（繁體）: Traditional Chinese');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'SwitchBetweenPortugueseDialectsTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(this.portugueseDoc);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('en-us', 'United States')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale('pt-br', 'português (Brasil): Brazil')
          .call(doCmd('nextLine'))
          .expectSpeechWithLocale('pt-pt', 'português (Portugal): Portugal');
      await mockFeedback.replay();
    });

// Tests logic in shouldAnnounceLocale_(). We only announce the locale once when
// transitioning to more specific locales, e.g. 'en' -> 'en-us'. Transitions to
// less specific locales, e.g. 'en-us' -> 'en' should not be announced. Finally,
// subsequent transitions to the same locale, e.g. 'en' -> 'en-us' should not be
// announced.
AX_TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'MaybeAnnounceLocale', async function() {
      const mockFeedback = this.createMockFeedback();
      await this.runWithLoadedTree(`
  <p lang="en">Start</p>
  <p lang="en-ca">Middle</p>
  <p lang="en">Penultimate</p>
  <p lang="en-ca">End</p>
  `);
      SettingsManager.set('languageSwitching', true);
      this.setAvailableVoices();
      mockFeedback.call(doCmd('jumpToTop'))
          .expectSpeechWithLocale('en', 'Start')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('en-ca', 'English (Canada): Middle')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('en', 'Penultimate')
          .call(doCmd('nextObject'))
          .expectSpeechWithLocale('en-ca', 'End');
      await mockFeedback.replay();
    });
