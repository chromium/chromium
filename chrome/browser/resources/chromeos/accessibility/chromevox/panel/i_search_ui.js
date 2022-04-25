// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The UI for searching the ChromeVox panel menus incrementally.
 */

import {PanelInterface} from './panel_interface.js';

export class ISearchUI {
  /**
   * @param {Element} input
   */
  constructor(input) {
    /** @private {Element} */
    this.input_ = input;
    /** @private {constants.Dir} */
    this.dir_ = constants.Dir.FORWARD;

    this.onKeyDown = (event) => this.onKeyDown_(event);
    this.onTextInput = (event) => this.onTextInput_(event);

    input.addEventListener('keydown', this.onKeyDown, true);
    input.addEventListener('textInput', this.onTextInput, false);
  }

  /** @param {Element} input */
  static async init(input) {
    if (ISearchUI.instance_) {
      ISearchUI.instance_.destroy();
    }

    if (!input) {
      throw 'Expected search input';
    }

    await BackgroundBridge.PanelBackground.createNewISearch();
    ISearchUI.instance_ = new ISearchUI(input);
    input.focus();
    input.select();
  }

  /**
   * Listens to key down events.
   * @param {Event} evt
   * @return {boolean}
   * @private
   */
  onKeyDown_(evt) {
    switch (evt.key) {
      case 'ArrowUp':
        this.dir_ = constants.Dir.BACKWARD;
        break;
      case 'ArrowDown':
        this.dir_ = constants.Dir.FORWARD;
        break;
      case 'Escape':
        PanelInterface.instance.closeMenusAndRestoreFocus();
        return false;
      case 'Enter':
        PanelInterface.instance.setPendingCallback(
            async () =>
                await BackgroundBridge.PanelBackground.setRangeToISearchNode());
        PanelInterface.instance.closeMenusAndRestoreFocus();
        return false;
      default:
        return false;
    }
    BackgroundBridge.PanelBackground.incrementalSearch(
        this.input_.value, this.dir_, true);
    evt.preventDefault();
    evt.stopPropagation();
    return false;
  }

  /**
   * Listens to text input events.
   * @param {Event} evt
   * @return {boolean}
   * @private
   */
  onTextInput_(evt) {
    const searchStr = evt.target.value + evt.data;
    BackgroundBridge.PanelBackground.incrementalSearch(searchStr, this.dir_);
    return true;
  }

  /** Unregisters event handlers. */
  destroy() {
    const input = this.input_;
    this.input_ = null;
    input.removeEventListener('keydown', this.onKeyDown, true);
    input.removeEventListener('textInput', this.onTextInput, false);
  }
}
