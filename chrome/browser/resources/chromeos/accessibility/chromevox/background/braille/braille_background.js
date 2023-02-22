// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */
import {BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {LogType} from '../../common/log_types.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {LogStore} from '../logging/log_store.js';

import {BrailleDisplayManager} from './braille_display_manager.js';
import {BrailleInputHandler} from './braille_input_handler.js';
import {BrailleInterface} from './braille_interface.js';
import {BrailleKeyEventRewriter} from './braille_key_event_rewriter.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';

/** @implements {BrailleInterface} */
export class BrailleBackground {
  constructor() {
    /** @private {!BrailleDisplayManager} */
    this.displayManager_ = new BrailleDisplayManager();
    this.displayManager_.setCommandListener(
        (evt, content) => this.onBrailleKeyEvent_(evt, content));

    /** @private {boolean} */
    this.frozen_ = false;

    /** @private {!BrailleInputHandler} */
    this.inputHandler_ = new BrailleInputHandler();

    /** @private {BrailleKeyEventRewriter} */
    this.keyEventRewriter_ = new BrailleKeyEventRewriter();

    /** @private {NavBraille} */
    this.lastContent_ = null;
    /** @private {?string} */
    this.lastContentId_ = null;
  }

  static init() {
    if (BrailleBackground.instance) {
      throw new Error('Cannot create two BrailleBackground instances');
    }
    // Must be called before BrailleBackground is constructed.
    BrailleTranslatorManager.init();

    BrailleBackground.instance = new BrailleBackground();
  }

  /** @override */
  write(params) {
    if (this.frozen_) {
      return;
    }

    if (SettingsManager.getBoolean('enableBrailleLogging')) {
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
      BrailleTranslatorManager.instance.getDefaultTranslator().backTranslate(
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
