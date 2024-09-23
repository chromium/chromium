// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BrailleDisplayState, BrailleKeyEvent} from '../../common/braille/braille_key_types.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {LogStore} from '../logging/log_store.js';

import {BrailleDisplayManager} from './braille_display_manager.js';
import {BrailleInputHandler} from './braille_input_handler.js';
import {BrailleInterface} from './braille_interface.js';
import {BrailleKeyEventRewriter} from './braille_key_event_rewriter.js';
import {BrailleTranslatorManager} from './braille_translator_manager.js';

export class BrailleBackground implements BrailleInterface {
  private frozen_ = false;

  static instance: BrailleBackground;

  constructor() {
    BrailleDisplayManager.instance.setCommandListener(
        (evt, content) => this.routeBrailleKeyEvent_(evt, content));
  }

  static init(): void {
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

  /** BrailleInterface implementation. */
  write(params: NavBraille): void {
    if (this.frozen_) {
      return;
    }

    LogStore.instance.writeBrailleLog(params.text.toString());
    this.setContent_(params, null);
  }

  /** BrailleInterface implementation. */
  writeRawImage(imageDataUrl: string): void {
    if (this.frozen_) {
      return;
    }
    BrailleDisplayManager.instance.setImageContent(imageDataUrl);
  }

  /** BrailleInterface implementation. */
  freeze(): void {
    this.frozen_ = true;
  }

  /** BrailleInterface implementation. */
  thaw(): void {
    this.frozen_ = false;
  }

  /** BrailleInterface implementation. */
  getDisplayState(): BrailleDisplayState {
    return BrailleDisplayManager.instance.getDisplayState();
  }

  /** BrailleInterface implementation. */
  panLeft(): void {
    BrailleDisplayManager.instance.panLeft();
  }

  /** BrailleInterface implementation. */
  panRight(): void {
    BrailleDisplayManager.instance.panRight();
  }

  /** BrailleInterface implementation. */
  route(displayPosition: number | undefined): void {
    return BrailleDisplayManager.instance.route(displayPosition);
  }

  /** BrailleInterface implementation. */
  async backTranslate(cells: ArrayBuffer): Promise<string | null> {
    return await BrailleTranslatorManager.backTranslate(cells);
  }

  private setContent_(
      newContent: NavBraille, _newContentId: string | null): void {
    const updateContent = (): void => BrailleDisplayManager.instance.setContent(
        newContent, BrailleInputHandler.instance.getExpansionType());
    BrailleInputHandler.instance.onDisplayContentChanged(
        newContent.text, updateContent);
    updateContent();
  }

  /**
   * Handles braille key events by dispatching either to the event rewriter,
   * input handler, or ChromeVox's background object.
   * @param content Content of display when event fired.
   */
  private routeBrailleKeyEvent_(
      brailleEvt: BrailleKeyEvent, content: NavBraille): void {
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

TestImportManager.exportForTesting(BrailleBackground);
