// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js',
]);

/**
 * Test fixture for ChromeVox options page.
 */
ChromeVoxOptionsTest = class extends ChromeVoxE2ETest {
  constructor() {
    super();
    globalThis.EventType = chrome.automation.EventType;
    globalThis.RoleType = chrome.automation.RoleType;
    globalThis.press = this.press;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      // Alphabetical based on file path.
      importModule(
          'CommandHandlerInterface',
          '/chromevox/background/command_handler_interface.js'),
      importModule('TtsSettings', '/chromevox/common/tts_types.js'),
      importModule('AsyncUtil', '/common/async_util.js'),
      importModule('EventGenerator', '/common/event_generator.js'),
      importModule('KeyCode', '/common/key_code.js'),
      importModule('LocalStorage', '/common/local_storage.js'),
      importModule('SettingsManager', '/chromevox/common/settings_manager.js'),
    ]);
  }

  async loadOptionsPage() {
    return new Promise(async resolve => {
      const mockFeedback = this.createMockFeedback();
      const desktop = await AsyncUtil.getDesktop();
      desktop.addEventListener(
          EventType.LOAD_COMPLETE, evt => {
            if (evt.target.docUrl.indexOf('options/options.html') === -1 ||
                !evt.target.docLoaded) {
              return;
            }

            mockFeedback.expectSpeech('ChromeVox Options');
            resolve([mockFeedback, evt]);
          });
      CommandHandlerInterface.instance.onCommand('showOptionsPage');
    });
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }
};

// TODO(crbug.com/1318133): Test times out flakily.
AX_TEST_F(
    'ChromeVoxOptionsTest', 'DISABLED_NumberReadingStyleSelect',
    async function() {
      const [mockFeedback, evt] = await this.loadOptionsPage();
      const numberStyleSelect = evt.target.find({
        role: RoleType.POP_UP_BUTTON,
        attributes: {name: 'Read numbers as:'},
      });
      assertNotNullNorUndefined(numberStyleSelect);
      mockFeedback.call(numberStyleSelect.focus.bind(numberStyleSelect))
          .expectSpeech('Read numbers as:', 'Words', 'Collapsed')
          .call(numberStyleSelect.doDefault.bind(numberStyleSelect))
          .expectSpeech('Expanded')

          // Before selecting the menu option.
          .call(() => {
            assertEquals('asWords', SettingsManager.get('numberReadingStyle'));
          })

          .call(press(KeyCode.DOWN))
          .expectSpeech('Digits', 'List item', ' 2 of 2 ')
          .call(press(KeyCode.RETURN))
          .expectSpeech('Digits', 'Collapsed')
          .call(() => {
            assertEquals('asDigits', SettingsManager.get('numberReadingStyle'));
          });

      await mockFeedback.replay();
    });

// TODO(crbug.com/1128926, crbug.com/1172387):
// Test times out flakily.
AX_TEST_F(
    'ChromeVoxOptionsTest', 'DISABLED_PunctuationEchoSelect', async function() {
      const [mockFeedback, evt] = await this.loadOptionsPage();
      const PUNCTUATION_ECHO_NONE = '0';
      const PUNCTUATION_ECHO_SOME = '1';
      const PUNCTUATION_ECHO_ALL = '2';
      const punctuationEchoSelect = evt.target.find({
        role: RoleType.POP_UP_BUTTON,
        attributes: {name: 'Punctuation echo:'},
      });
      assertNotNullNorUndefined(punctuationEchoSelect);
      mockFeedback.call(punctuationEchoSelect.focus.bind(punctuationEchoSelect))
          .expectSpeech('Punctuation echo:', 'None', 'Collapsed')
          .call(punctuationEchoSelect.doDefault.bind(punctuationEchoSelect))
          .expectSpeech('Expanded')

          // Before selecting the menu option.
          .call(() => {
            assertEquals(
                PUNCTUATION_ECHO_NONE,
                SettingsManager.get(TtsSettings.PUNCTUATION_ECHO));
          })

          .call(press(KeyCode.DOWN))
          .expectSpeech('Some', 'List item', ' 2 of 3 ')
          .call(press(KeyCode.RETURN))
          .expectSpeech('Some', 'Collapsed')
          .call(() => {
            assertEquals(
                PUNCTUATION_ECHO_SOME,
                SettingsManager.get(TtsSettings.PUNCTUATION_ECHO));
          })

          .call(press(KeyCode.DOWN))
          .expectSpeech('All', ' 3 of 3 ')
          .call(() => {
            assertEquals(
                PUNCTUATION_ECHO_ALL,
                SettingsManager.get(TtsSettings.PUNCTUATION_ECHO));
          });

      await mockFeedback.replay();
    });

// TODO(crbug.com/1128926, crbug.com/1172387):
// Test times out flakily.
AX_TEST_F('ChromeVoxOptionsTest', 'DISABLED_SmartStickyMode', async function() {
  const [mockFeedback, evt] = await this.loadOptionsPage();
  const smartStickyModeCheckbox = evt.target.find({
    role: RoleType.CHECK_BOX,
    attributes:
        {name: 'Turn off sticky mode when editing text (Smart Sticky Mode)'},
  });
  assertNotNullNorUndefined(smartStickyModeCheckbox);
  mockFeedback.call(smartStickyModeCheckbox.focus.bind(smartStickyModeCheckbox))
      .expectSpeech(
          'Turn off sticky mode when editing text (Smart Sticky Mode)',
          'Check box', 'Checked')
      .call(() => {
        assertEquals('true', SettingsManager.get('smartStickyMode'));
      })
      .call(smartStickyModeCheckbox.doDefault.bind(smartStickyModeCheckbox))
      .expectSpeech(
          'Turn off sticky mode when editing text (Smart Sticky Mode)',
          'Check box', 'Not checked')
      .call(() => {
        assertEquals('false', SettingsManager.get('smartStickyMode'));
      });

  await mockFeedback.replay();
});

// TODO(crbug.com/1169396, crbug.com/1172387):
// Test times out or crashes flakily.
AX_TEST_F('ChromeVoxOptionsTest', 'DISABLED_UsePitchChanges', async function() {
  const [mockFeedback, evt] = await this.loadOptionsPage();
  const pitchChangesCheckbox = evt.target.find({
    role: RoleType.CHECK_BOX,
    attributes: {
      name: 'Change pitch when speaking element types and quoted, ' +
          'deleted, bolded, parenthesized, or capitalized text.',
    },
  });
  const capitalStrategySelect = evt.target.find({
    role: RoleType.POP_UP_BUTTON,
    attributes: {name: 'When reading capitals:'},
  });
  assertNotNullNorUndefined(pitchChangesCheckbox);
  assertNotNullNorUndefined(capitalStrategySelect);

  // Assert initial pref values.
  assertTrue(SettingsManager.getBoolean('usePitchChanges'));
  assertEquals('increasePitch', SettingsManager.getString('capitalStrategy'));

  mockFeedback.call(pitchChangesCheckbox.focus.bind(pitchChangesCheckbox))
      .expectSpeech(
          'Change pitch when speaking element types and quoted, ' +
              'deleted, bolded, parenthesized, or capitalized text.',
          'Check box', 'Checked')
      .call(pitchChangesCheckbox.doDefault.bind(pitchChangesCheckbox))
      .expectSpeech(
          'Change pitch when speaking element types and quoted, ' +
              'deleted, bolded, parenthesized, or capitalized text.',
          'Check box', 'Not checked')
      .call(() => {
        assertFalse(SettingsManager.getBoolean('usePitchChanges'));
        // Toggling usePitchChanges affects capitalStrategy. Ensure that
        // the preference has been changed and that the 'Increase pitch'
        // option is hidden.
        assertEquals(
            'announceCapitals', SettingsManager.getString('capitalStrategy'));

        // Open the menu first in order to assert this.
        // const increasePitchOption = evt.target.find({
        //  role: RoleType.MENU_LIST_OPTION,
        //  attributes: {name: 'Increase pitch'}
        //});
        // assertNotNullNorUndefined(increasePitchOption);
        // assertTrue(increasePitchOption.state.invisible);
      })
      .call(capitalStrategySelect.focus.bind(capitalStrategySelect))
      .expectSpeech(
          'When reading capitals:', 'Speak "cap" before letter', 'Collapsed')
      .call(pitchChangesCheckbox.doDefault.bind(pitchChangesCheckbox))
      .expectSpeech(
          'Change pitch when speaking element types and quoted, ' +
              'deleted, bolded, parenthesized, or capitalized text.',
          'Check box', 'Checked')
      .call(() => {
        assertTrue(SettingsManager.getBoolean('usePitchChanges'));
        // Ensure that the capitalStrategy preference is restored to its
        // initial setting and that the 'Increase pitch' option is visible
        // again.
        assertEquals(
            'increasePitch', SettingsManager.getString('capitalStrategy'));

        // Open the menu first in order to assert this.
        // const increasePitchOption = evt.target.find({
        //  role: RoleType.MENU_LIST_OPTION,
        //  attributes: {name: 'Increase pitch'}
        //});
        // assertNotNullNorUndefined(increasePitchOption);
        // assertEquals(undefined, increasePitchOption.state.invisible);
      })
      .call(capitalStrategySelect.focus.bind(capitalStrategySelect))
      .expectSpeech('When reading capitals:', 'Increase pitch', 'Collapsed');
  await mockFeedback.replay();
});
