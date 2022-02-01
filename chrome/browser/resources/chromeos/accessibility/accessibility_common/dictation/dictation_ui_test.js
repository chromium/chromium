// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/** UI tests for Dictation. */
DictationUIE2ETest = class extends DictationE2ETestBase {
  constructor() {
    super();

    this.iconType = this.mockAccessibilityPrivate.DictationBubbleIconType;
  }

  /**
   * Returns true if `targetProps` matches the most recent UI properties. Must
   * match exactly.
   * @param {DictationBubbleProperties} targetProps
   * @return {boolean}
   */
  uiPropertiesMatch(targetProps) {
    /** @type {function(!Array<string>,!Array<string>) : boolean} */
    const areEqual = (arr1, arr2) => {
      return arr1.every((val, index) => val === arr2[index]);
    };

    const actualProps = this.mockAccessibilityPrivate.getDictationBubbleProps();
    if (!actualProps) {
      return false;
    }

    if (Object.keys(actualProps).length !== Object.keys(targetProps).length) {
      return false;
    }

    for (const key of Object.keys(targetProps)) {
      if (Array.isArray(targetProps[key]) && Array.isArray(actualProps[key])) {
        // For arrays, ensure that we compare the contents of the arrays.
        if (!areEqual(targetProps[key], actualProps[key])) {
          return false;
        }
      } else if (targetProps[key] !== actualProps[key]) {
        return false;
      }
    }

    return true;
  }

  /**
   * Waits for the updateDictationBubble() API to be called with the given
   * properties.
   * @param {DictationBubbleProperties} targetProps
   */
  async waitForUIProperties(targetProps) {
    // Poll until the updateDictationBubble() API gets called with
    // `targetProps`.
    return new Promise(resolve => {
      const intervalId = setInterval(() => {
        if (this.uiPropertiesMatch(targetProps)) {
          clearInterval(intervalId);
          resolve();
        }
      });
    });
  }
};

SYNC_TEST_F(
    'DictationUIE2ETest', 'ShownWhenSpeechRecognitionStarts', async function() {
      await this.waitForDictationWithCommands();
      await this.toggleDictationAndStartListening(1);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        text: undefined,
        hints: undefined
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'DisplaysInterimSpeechResults', async function() {
      await this.waitForDictationWithCommands();
      await this.toggleDictationAndStartListening(1);
      // Send an interim speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing', /*isFinal=*/ false);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.HIDDEN,
        text: 'Testing',
        hints: undefined
      });
    });

SYNC_TEST_F('DictationUIE2ETest', 'DisplaysMacroSuccess', async function() {
  await this.waitForDictationWithCommands();
  await this.toggleDictationAndStartListening(1);
  // Perform a command.
  this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
      this.commandStrings.SELECT_ALL_TEXT, /*isFinal=*/ true);
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.MACRO_SUCCESS,
    text: this.commandStrings.SELECT_ALL_TEXT,
    hints: undefined
  });
  // UI should return to standby mode after a timeout.
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.STANDBY,
    text: undefined,
    hints: undefined
  });
});

SYNC_TEST_F(
    'DictationUIE2ETest', 'ResetsToStandbyModeAfterFinalSpeechResult',
    async function() {
      await this.waitForDictationWithCommands();
      await this.toggleDictationAndStartListening(1);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        text: undefined,
        hints: undefined
      });
      // Send an interim speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing', /*isFinal=*/ false);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.HIDDEN,
        text: 'Testing',
        hints: undefined
      });
      // Send a final speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing 123', /*isFinal=*/ true);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        text: undefined,
        hints: undefined
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'HiddenWhenDictationDeactivates', async function() {
      await this.waitForDictationWithCommands();
      await this.toggleDictationAndStartListening(1);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        text: undefined,
        hints: undefined
      });
      this.toggleDictationOffFromA11yPrivate();
      await this.waitForUIProperties(
          {visible: false, icon: this.iconType.HIDDEN});
    });

SYNC_TEST_F('DictationUIE2ETest', 'Hints', async function() {
  await this.waitForDictationWithCommandsAndHints();
  await this.toggleDictationAndStartListening(1);
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.STANDBY,
    text: undefined,
    hints: undefined
  });
  // Hints should show up after a few seconds without speech.
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.STANDBY,
    text: undefined,
    hints: ['Sample hint']
  });
});
