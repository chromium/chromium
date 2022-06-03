// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */

goog.provide('BrailleBackground');

goog.require('BrailleKeyEventRewriter');
goog.require('ChromeVoxState');
goog.require('LogStore');
goog.require('BrailleDisplayManager');
goog.require('BrailleInputHandler');
goog.require('BrailleInterface');
goog.require('BrailleKeyEvent');
goog.require('BrailleTranslatorManager');

/**
 * @implements {BrailleInterface}
 */
BrailleBackground = class {
  /**
   * @param {BrailleDisplayManager=} opt_displayManagerForTest
   *        Display manager (for mocking in tests).
   * @param {BrailleInputHandler=} opt_inputHandlerForTest Input handler
   *        (for mocking in tests).
   * @param {BrailleTranslatorManager=} opt_translatorManagerForTest
   *        Braille translator manager (for mocking in tests)
   */
  constructor(
      opt_displayManagerForTest, opt_inputHandlerForTest,
      opt_translatorManagerForTest) {
    /**
     * @type {!BrailleTranslatorManager}
     * @private
     */
    this.translatorManager_ =
        opt_translatorManagerForTest || new BrailleTranslatorManager();
    /**
     * @type {BrailleDisplayManager}
     * @private
     */
    this.displayManager_ = opt_displayManagerForTest ||
        new BrailleDisplayManager(this.translatorManager_);
    this.displayManager_.setCommandListener(this.onBrailleKeyEvent_.bind(this));
    /**
     * @type {NavBraille}
     * @private
     */
    this.lastContent_ = null;
    /**
     * @type {?string}
     * @private
     */
    this.lastContentId_ = null;
    /**
     * @type {!BrailleInputHandler}
     * @private
     */
    this.inputHandler_ = opt_inputHandlerForTest ||
        new BrailleInputHandler(this.translatorManager_);
    this.inputHandler_.init();

    /** @private {boolean} */
    this.frozen_ = false;

    /** @private {BrailleKeyEventRewriter} */
    this.keyEventRewriter_ = new BrailleKeyEventRewriter();
  }

  /** @override */
  write(params) {
    if (this.frozen_) {
      return;
    }

    if (localStorage['enableBrailleLogging'] === 'true') {
      const logStr = 'Braille "' + params.text.toString() + '"';
      LogStore.getInstance().writeTextLog(logStr, LogStore.LogType.BRAILLE);
      console.log(logStr);
    }

    this.setContent_(params, null);
  }

  /** @override */
  writeRawImage(imageDataUrl) {
    if (this.frozen_) {
      return;
    }
    this.displayManager_.setImageContent(imageDataUrl);
  }

  /** @override */
  freeze() {
    this.frozen_ = true;
  }

  /** @override */
  thaw() {
    this.frozen_ = false;
  }

  /** @override */
  getDisplayState() {
    return this.displayManager_.getDisplayState();
  }

  /**
   * @return {BrailleTranslatorManager} The translator manager used by this
   *     instance.
   */
  getTranslatorManager() {
    return this.translatorManager_;
  }

  /** @override */
  panLeft() {
    this.displayManager_.panLeft();
  }

  /** @override */
  panRight() {
    this.displayManager_.panRight();
  }

  /** @override */
  route(displayPosition) {
    return this.displayManager_.route(displayPosition);
  }

  /**
   * @param {!NavBraille} newContent
   * @param {?string} newContentId
   * @private
   */
  setContent_(newContent, newContentId) {
    const updateContent = function() {
      this.lastContent_ = newContentId ? newContent : null;
      this.lastContentId_ = newContentId;
      this.displayManager_.setContent(
          newContent, this.inputHandler_.getExpansionType());
    }.bind(this);
    this.inputHandler_.onDisplayContentChanged(newContent.text, updateContent);
    updateContent();
  }

  /**
   * Handles braille key events by dispatching either to the input handler,
   * ChromeVox next's background object or ChromeVox classic's content script.
   * @param {!BrailleKeyEvent} brailleEvt The event.
   * @param {!NavBraille} content Content of display when event fired.
   * @private
   */
  onBrailleKeyEvent_(brailleEvt, content) {
    if (this.keyEventRewriter_.onBrailleKeyEvent(brailleEvt)) {
      return;
    }

    if (this.inputHandler_.onBrailleKeyEvent(brailleEvt)) {
      return;
    }
    if (ChromeVoxState.instance &&
        ChromeVoxState.instance.onBrailleKeyEvent(brailleEvt, content)) {
      return;
    }
  }
};
goog.addSingletonGetter(BrailleBackground);
