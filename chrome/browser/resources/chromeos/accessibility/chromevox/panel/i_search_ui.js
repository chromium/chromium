// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The driver for the UI for incremental search.
 */
import {constants} from '../../common/constants.js';
import {BackgroundBridge} from '../common/background_bridge.js';

import {PanelInterface} from './panel_interface.js';

const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;

export class ISearchUI {
  /** @param {!Element} input */
  constructor(input) {
    this.input_ = input;
    this.dir_ = Dir.FORWARD;

    this.onKeyDown = this.onKeyDown.bind(this);
    this.onTextInput = this.onTextInput.bind(this);

    input.addEventListener('keydown', this.onKeyDown, true);
    input.addEventListener('textInput', this.onTextInput, false);
  }

  /**
   * @param {!Element} input
   * @return {!Promise<ISearchUI>}
   */
  static async init(input) {
    if (ISearchUI.instance) {
      ISearchUI.instance.destroy();
    }

    await BackgroundBridge.PanelBackground.createNewISearch();
    ISearchUI.instance = new ISearchUI(input);
    input.focus();
    input.select();
    return ISearchUI.instance;
  }

  /**
   * Listens to key down events.
   * @param {Event} evt
   * @return {boolean}
   */
  onKeyDown(evt) {
    switch (evt.key) {
      case 'ArrowUp':
        this.dir_ = Dir.BACKWARD;
        break;
      case 'ArrowDown':
        this.dir_ = Dir.FORWARD;
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
   */
  onTextInput(evt) {
    const searchStr = evt.target.value + evt.data;
    BackgroundBridge.PanelBackground.incrementalSearch(searchStr, this.dir_);
    return true;
  }

  /** Unregisters event handlers. */
  destroy() {
    BackgroundBridge.PanelBackground.destroyISearch();
    const input = this.input_;
    this.input_ = null;
    input.removeEventListener('keydown', this.onKeyDown, true);
    input.removeEventListener('textInput', this.onTextInput, false);
  }
}

/** @type {ISearchUI} */
ISearchUI.instance;
