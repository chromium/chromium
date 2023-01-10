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

  /** Waits for the FocusHandler to initialize. */
  async waitForFocusHandler() {
    const focusHandler = this.dictation_.focusHandler_;
    const isReady = () => {
      return focusHandler.isReadyForTesting();
    };

    if (isReady()) {
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      // Wait for focusHandler to be active and have a valid editable node by
      // attaching the necessary event listeners.
      const onPropertyChanged = () => {
        if (isReady()) {
          focusHandler.onActiveChangedForTesting_ = null;
          focusHandler.onEditableNodeChangedForTesting_ = null;
          resolve();
        }
      };

      focusHandler.onActiveChangedForTesting_ = onPropertyChanged;
      focusHandler.onEditableNodeChangedForTesting_ = onPropertyChanged;
    });

    this.notifyCcTests_();
  }

  /** Waits for the SandboxedPumpkinTagger to initialize. */
  async WaitForPumpkinTaggerReady() {
    const strategy = this.dictation_.speechParser_.pumpkinParseStrategy_;
    const isReady = () => {
      return strategy.pumpkinTaggerReady_;
    };

    if (isReady()) {
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      // Wait for SandboxedPumpkinTagger to initialize by attaching the
      // necessary event listener.
      const onPropertyChanged = () => {
        if (isReady()) {
          strategy.onPumpkinTaggerReadyChangedForTesting_ = null;
          resolve();
        }
      };
      strategy.onPumpkinTaggerReadyChangedForTesting_ = onPropertyChanged;
    });

    this.notifyCcTests_();
  }

  /** @param {string} value */
  async waitForEditableValue(value) {
    const inputController = this.dictation_.inputController_;
    const goalTest = () => {
      const data = inputController.getEditableNodeData();
      return data && data.value === value;
    };

    if (goalTest()) {
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      // Wait for the editable value by attaching the necessary event listener.
      const editableNode = inputController.getEditableNodeData().node;
      const onValueChanged = () => {
        if (goalTest()) {
          editableNode.removeEventListener(
              chrome.automation.EventType.VALUE_IN_TEXT_FIELD_CHANGED,
              onValueChanged, false);
          resolve();
        }
      };

      editableNode.addEventListener(
          chrome.automation.EventType.VALUE_IN_TEXT_FIELD_CHANGED,
          onValueChanged, false);
    });

    this.notifyCcTests_();
  }
}

globalThis.testSupport = new DictationTestSupport();
