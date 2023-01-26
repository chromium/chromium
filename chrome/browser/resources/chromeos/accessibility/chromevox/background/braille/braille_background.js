// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */
import {LocalStorage} from '../../../common/local_storage.js';
import {BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {BridgeHelper} from '../../common/bridge_helper.js';
import {LogType} from '../../common/log_types.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {LogStore} from '../logging/log_store.js';

import {BrailleDisplayManager} from './braille_display_manager.js';
import {BrailleInputHandler} from './braille_input_handler.js';
import {BrailleInterface} from './braille_interface.js';
import {BrailleKeyEventRewriter} from './braille_key_event_rewriter.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';

const Action = BridgeConstants.BrailleBackground.Action;
const TARGET = BridgeConstants.BrailleBackground.TARGET;

/** @implements {BrailleInterface} */
export class BrailleBackground {
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
    /** @private {!BrailleTranslatorManager} */
    this.translatorManager_ =
        opt_translatorManagerForTest || new BrailleTranslatorManager();
    /** @private {!BrailleDisplayManager} */
    this.displayManager_ = opt_displayManagerForTest ||
        new BrailleDisplayManager(this.translatorManager_);
    this.displayManager_.setCommandListener(
        (evt, content) => this.onBrailleKeyEvent_(evt, content));

    /** @private {boolean} */
    this.frozen_ = false;

    /** @private {!BrailleInputHandler} */
    this.inputHandler_ = opt_inputHandlerForTest ||
        new BrailleInputHandler(this.translatorManager_);

    /** @private {BrailleKeyEventRewriter} */
    this.keyEventRewriter_ = new BrailleKeyEventRewriter();

    /** @private {NavBraille} */
    this.lastContent_ = null;
    /** @private {?string} */
    this.lastContentId_ = null;
  }

  static init() {
    BrailleBackground.instance = new BrailleBackground();

    BridgeHelper.registerHandler(
        TARGET, Action.REFRESH_BRAILLE_TABLE,
        brailleTable =>
            BrailleBackground.instance.getTranslatorManager().refresh(
                brailleTable));
  }

  /** @override */
  write(params) {
    if (this.frozen_) {
      return;
    }

    if (LocalStorage.get('enableBrailleLogging')) {
      const logStr = 'Braille "' + params.text.toString() + '"';
      LogStore.instance.writeTextLog(logStr, LogType.BRAILLE);
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

  /** @override */
  async backTranslate(cells) {
    return new Promise(resolve => {
      this.translatorManager_.getDefaultTranslator().backTranslate(
          cells, resolve);
    });
  }

  /**
   * @param {!NavBraille} newContent
   * @param {?string} newContentId
   * @private
   */
  setContent_(newContent, newContentId) {
    const updateContent = () => {
      this.lastContent_ = newContentId ? newContent : null;
      this.lastContentId_ = newContentId;
      this.displayManager_.setContent(
          newContent, this.inputHandler_.getExpansionType());
    };
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
    if (ChromeVoxState.instance) {
      ChromeVoxState.instance.onBrailleKeyEvent(brailleEvt, content);
    }
  }
}

/** @type {BrailleBackground} */
BrailleBackground.instance;
