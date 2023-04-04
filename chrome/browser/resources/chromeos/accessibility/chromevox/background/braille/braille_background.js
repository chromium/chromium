// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */
import {BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {LogType} from '../../common/log_types.js';
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
    /** @private {boolean} */
    this.frozen_ = false;

    /** @private {NavBraille} */
    this.lastContent_ = null;
    /** @private {?string} */
    this.lastContentId_ = null;

    BrailleDisplayManager.instance.setCommandListener(
        (evt, content) => this.routeBrailleKeyEvent_(evt, content));
  }

  static init() {
    if (BrailleBackground.instance) {
      throw new Error('Cannot create two BrailleBackground instances');
    }
    // Must be called first.
    BrailleTranslatorManager.init();

    // Must be called before creating BrailleBackground.
    BrailleDisplayManager.init();
    BrailleInputHandler.init();
    BrailleKeyEventRewriter.init();

    BrailleBackground.instance = new BrailleBackground();
  }

  /** @override */
  write(params) {
    if (this.frozen_) {
      return;
    }

    LogStore.instance.writeBrailleLog(params.text.toString());
    this.setContent_(params, null);
  }

  /** @override */
  writeRawImage(imageDataUrl) {
    if (this.frozen_) {
      return;
    }
    BrailleDisplayManager.instance.setImageContent(imageDataUrl);
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
    return BrailleDisplayManager.instance.getDisplayState();
  }

  /** @override */
  panLeft() {
    BrailleDisplayManager.instance.panLeft();
  }

  /** @override */
  panRight() {
    BrailleDisplayManager.instance.panRight();
  }

  /** @override */
  route(displayPosition) {
    return BrailleDisplayManager.instance.route(displayPosition);
  }

  /** @override */
  async backTranslate(cells) {
    return await BrailleTranslatorManager.backTranslate(cells);
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
      BrailleDisplayManager.instance.setContent(
          newContent, BrailleInputHandler.instance.getExpansionType());
    };
    BrailleInputHandler.instance.onDisplayContentChanged(
        newContent.text, updateContent);
    updateContent();
  }

  /**
   * Handles braille key events by dispatching either to the event rewriter,
   * input handler, or ChromeVox's background object.
   * @param {!BrailleKeyEvent} brailleEvt The event.
   * @param {!NavBraille} content Content of display when event fired.
   * @private
   */
  routeBrailleKeyEvent_(brailleEvt, content) {
    if (BrailleKeyEventRewriter.instance.onBrailleKeyEvent(brailleEvt)) {
      return;
    }

    if (BrailleInputHandler.instance.onBrailleKeyEvent(brailleEvt)) {
      return;
    }
    if (ChromeVoxState.instance) {
      ChromeVoxState.instance.onBrailleKeyEvent(brailleEvt, content);
    }
  }
}

/** @type {BrailleBackground} */
BrailleBackground.instance;
