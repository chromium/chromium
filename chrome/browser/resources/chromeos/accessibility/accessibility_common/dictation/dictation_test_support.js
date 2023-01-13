// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A class that provides test support for C++ tests. */
class DictationTestSupport {
  constructor() {
    this.dictation_ = accessibilityCommon.dictation_;
    this.notifyCcTests_();
  }

  /**
   * Notifies C++ tests, which wait for the JS side to call
   * `domAutomationController.send`, that they can continue.
   * @private
   */
  notifyCcTests_() {
    domAutomationController.send('ready');
  }

  /** Increases Dictation timeouts for test stability. */
  increaseNoFocusedImeTimeout() {
    this.dictation_.increaseNoFocusedImeTimeoutForTesting();
    this.notifyCcTests_();
  }

  /** Disables Pumpkin for tests that use regex-based command parsing. */
  disablePumpkin() {
    this.dictation_.disablePumpkinForTesting();
    this.notifyCcTests_();
  }

  /**
   * TODO(b:264535324): Remove polling from this method.
   * Waits for the FocusHandler to initialize.
   */
  async waitForFocusHandler() {
    await new Promise(resolve => {
      const printErrorMessageTimeoutId = setTimeout(() => {
        console.error('Still waiting for FocusHandler');
      }, 3.5 * 1000);
      const intervalId = setInterval(() => {
        if (this.dictation_.focusHandler_.isReadyForTesting()) {
          clearTimeout(printErrorMessageTimeoutId);
          clearInterval(intervalId);
          resolve();
        }
      }, 500);
    });

    this.notifyCcTests_();
  }

  /**
   * TODO(b:264535324): Remove polling from this method.
   * Waits for the SandboxedPumpkinTagger to initialize.
   */
  async WaitForPumpkinTaggerReady() {
    await new Promise(resolve => {
      const printErrorMessageTimeoutId = setTimeout(() => {
        console.error('Still waiting for SandboxedPumpkinTagger');
      }, 3.5 * 1000);
      const intervalId = setInterval(() => {
        if (this.dictation_.speechParser_.pumpkinParseStrategy_
                .pumpkinTaggerReady_) {
          clearTimeout(printErrorMessageTimeoutId);
          clearInterval(intervalId);
          resolve();
        }
      }, 500);
    });

    this.notifyCcTests_();
  }

  /**
   * TODO(b:264535324): Remove polling from this method.
   * @param {string} value
   */
  async waitForEditableValue(value) {
    await new Promise(resolve => {
      const printErrorMessageTimeoutId = setTimeout(() => {
        console.error('Still waiting for editable value: ' + value);
      }, 3.5 * 1000);
      const intervalId = setInterval(() => {
        const data = this.dictation_.inputController_.getEditableNodeData();
        if (data && data.value === value) {
          clearTimeout(printErrorMessageTimeoutId);
          clearInterval(intervalId);
          resolve();
        }
      }, 500);
    });

    this.notifyCcTests_();
  }
}

globalThis.testSupport = new DictationTestSupport();
