// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A class that provides test support for C++ tests. */
class DictationTestSupport {
  constructor() {
    this.dictation_ = accessibilityCommon.dictation_;
    this.speechRecognitionPrivateStart_ = chrome.speechRecognitionPrivate.start;
    this.speechRecognitionPrivateStartCalls_ = 0;
    this.notifyCcTests_();
  }

  /**
   * Notifies C++ tests, which wait for the JS side to call
   * `chrome.test.sendScriptResult`, that they can continue.
   * @private
   */
  notifyCcTests_() {
    chrome.test.sendScriptResult('ready');
  }

  /**
   * TODO(b/301475127): Move this logic into AutomationTestSupport.
   * Waits for focus to land on the editable field used in Dictation C++ tests.
   */
  async waitForEditableFocus() {
    const desktop = await this.getDesktop_();
    const focus = await new Promise(resolve => {
      chrome.automation.getFocus(f => resolve(f));
    });
    const isCorrectNode = (node) => {
      return node && node.className === 'editableForDictation';
    };

    if (isCorrectNode(focus)) {
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      const onFocusChanged = (event) => {
        const newFocus = event.target;
        if (isCorrectNode(newFocus)) {
          desktop.removeEventListener(
              chrome.automation.EventType.FOCUS, onFocusChanged);
          resolve();
        }
      };

      desktop.addEventListener(
          chrome.automation.EventType.FOCUS, onFocusChanged);
    });

    this.notifyCcTests_();
  }

  /** Sets Dictation timeouts for test stability. */
  setNoFocusedImeTimeout(duration) {
    this.dictation_.setNoFocusedImeTimeoutForTesting(duration);
    this.notifyCcTests_();
  }

  /** Disables Pumpkin for tests that use regex-based command parsing. */
  disablePumpkin() {
    this.dictation_.disablePumpkinForTesting();
    this.notifyCcTests_();
  }

  /**
   * Waits for the FocusHandler to initialize.
   * @param {string} expectedClassName
   */
  async waitForFocusHandler(expectedClassName) {
    const focusHandler = this.dictation_.focusHandler_;
    const isReady = () => {
      return focusHandler.isReadyForTesting(expectedClassName);
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
  async waitForPumpkinTaggerReady() {
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
      // Wait for the editable value by attaching the necessary event listeners.
      const editableNode = inputController.getEditableNodeData().node;
      const onEditableValueChanged = () => {
        if (goalTest()) {
          inputController.onSurroundingTextChangedForTesting_ = null;
          editableNode.removeEventListener(
              chrome.automation.EventType.VALUE_IN_TEXT_FIELD_CHANGED,
              onEditableValueChanged);
          resolve();
        }
      };

      // Attach two event listeners: one for the VALUE_IN_TEXT_FIELD_CHANGED
      // accessibility event, and one for the onSurroundingTextChanged IME
      // event. The VALUE_IN_TEXT_FIELD_CHANGED event gets fired when the value
      // of a <textarea> or <input> is changed; however, it doesn't get fired
      // when the value of a content editable is changed. To support content
      // editables, we use the onSurroundingTextChanged IME events.
      inputController.onSurroundingTextChangedForTesting_ =
          onEditableValueChanged;
      editableNode.addEventListener(
          chrome.automation.EventType.VALUE_IN_TEXT_FIELD_CHANGED,
          onEditableValueChanged);
    });

    this.notifyCcTests_();
  }

  /**
   * @param {number} selStart
   * @param {number} selEnd
   */
  async setSelection(selStart, selEnd) {
    await this.dictation_.inputController_.setSelection_(selStart, selEnd);
    this.notifyCcTests_();
  }

  /**
   * @param {number} selStart
   * @param {number} selEnd
   */
  async waitForSelection(selStart, selEnd) {
    const desktop = await this.getDesktop_();
    const inputController = this.dictation_.inputController_;
    const goalTest = () => {
      const data = inputController.getEditableNodeData();
      return data && data.selStart === selStart && data.selEnd === selEnd;
    };

    if (goalTest()) {
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      const onSelectionChanged = () => {
        if (goalTest()) {
          inputController.onSelectionChangedForTesting_ = null;
          desktop.removeEventListener(
              chrome.automation.EventType.DOCUMENT_SELECTION_CHANGED,
              onSelectionChanged);
          resolve();
        }
      };

      inputController.onSelectionChangedForTesting_ = onSelectionChanged;
      desktop.addEventListener(
          chrome.automation.EventType.DOCUMENT_SELECTION_CHANGED,
          onSelectionChanged);
    });

    this.notifyCcTests_();
  }

  /**
   * Installs a fake chrome.speechRecognitionPrivate.start() API that counts how
   * many times it was called.
   */
  installFakeSpeechRecognitionPrivateStart() {
    chrome.speechRecognitionPrivate.start = () => {
      this.speechRecognitionPrivateStartCalls_ += 1;
    };
    this.notifyCcTests_();
  }

  /**
   * Ensures that no calls to chrome.speechRecognitionPrivate.start() were made.
   */
  ensureNoSpeechRecognitionPrivateStartCalls() {
    if (this.speechRecognitionPrivateStartCalls_ === 0) {
      this.notifyCcTests_();
    }
  }

  /**
   * Restores the chrome.speechRecognitionPrivate.start() API to its production
   * implementation.
   */
  restoreSpeechRecognitionPrivateStart() {
    this.speechRecognitionPrivateStartCalls_ = 0;
    chrome.speechRecognitionPrivate.start = this.speechRecognitionPrivateStart_;
    this.notifyCcTests_();
  }

  async getDesktop_() {
    return new Promise(resolve => {
      chrome.automation.getDesktop(d => resolve(d));
    });
  }
}

globalThis.dictationTestSupport = new DictationTestSupport();
