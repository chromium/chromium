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
 * @constructor
 * @param {BrailleDisplayManager=} opt_displayManagerForTest
 *        Display manager (for mocking in tests).
 * @param {BrailleInputHandler=} opt_inputHandlerForTest Input handler
 *        (for mocking in tests).
 * @param {BrailleTranslatorManager=} opt_translatorManagerForTest
 *        Braille translator manager (for mocking in tests)
 * @implements {BrailleInterface}
 */
BrailleBackground = function(
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
};
goog.addSingletonGetter(BrailleBackground);


/** @override */
BrailleBackground.prototype.write = function(params) {
  if (this.frozen_) {
    return;
  }

  if (localStorage['enableBrailleLogging'] == 'true') {
    var logStr = 'Braille "' + params.text.toString() + '"';
    LogStore.getInstance().writeTextLog(logStr, LogStore.LogType.BRAILLE);
    console.log(logStr);
  }

  this.setContent_(params, null);
};


/** @override */
BrailleBackground.prototype.writeRawImage = function(imageDataUrl) {
  if (this.frozen_) {
    return;
  }
  this.displayManager_.setImageContent(imageDataUrl);
};


/** @override */
BrailleBackground.prototype.freeze = function() {
  this.frozen_ = true;
};


/** @override */
BrailleBackground.prototype.thaw = function() {
  this.frozen_ = false;
};


/** @override */
BrailleBackground.prototype.getDisplayState = function() {
  return this.displayManager_.getDisplayState();
};


/**
 * @return {BrailleTranslatorManager} The translator manager used by this
 *     instance.
 */
BrailleBackground.prototype.getTranslatorManager = function() {
  return this.translatorManager_;
};


/**
 * @param {!NavBraille} newContent
 * @param {?string} newContentId
 * @private
 */
BrailleBackground.prototype.setContent_ = function(newContent, newContentId) {
  var updateContent = function() {
    this.lastContent_ = newContentId ? newContent : null;
    this.lastContentId_ = newContentId;
    this.displayManager_.setContent(
        newContent, this.inputHandler_.getExpansionType());
  }.bind(this);
  this.inputHandler_.onDisplayContentChanged(newContent.text, updateContent);
  updateContent();
};


/**
 * Handles braille key events by dispatching either to the input handler,
 * ChromeVox next's background object or ChromeVox classic's content script.
 * @param {!BrailleKeyEvent} brailleEvt The event.
 * @param {!NavBraille} content Content of display when event fired.
 * @private
 */
BrailleBackground.prototype.onBrailleKeyEvent_ = function(brailleEvt, content) {
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
};
