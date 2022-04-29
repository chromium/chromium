// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The driver for the UI for incremental search.
 */
import {ISearchHandler} from '/chromevox/background/panel/i_search_handler.js';
import {PanelBackground} from '/chromevox/background/panel/panel_background.js';
import {PanelInterface} from '/chromevox/panel/panel_interface.js';

const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;

/** @implements {ISearchHandler} */
export class ISearchUI {
  /** @param {Element} input */
  constructor(input) {
    /** @private {ChromeVoxState} */
    this.background_ =
        chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
    const panelBackground =
        chrome.extension.getBackgroundPage().panelBackground;
    panelBackground.createNewISearch();
    panelBackground.setISearchHandler(this);
    this.input_ = input;
    this.dir_ = Dir.FORWARD;

    this.onKeyDown = this.onKeyDown.bind(this);
    this.onTextInput = this.onTextInput.bind(this);

    input.addEventListener('keydown', this.onKeyDown, true);
    input.addEventListener('textInput', this.onTextInput, false);
  }

  /**
   * @param {Element} input
   * @return {ISearchUI}
   */
  static init(input) {
    if (ISearchUI.instance_) {
      ISearchUI.instance_.destroy();
    }

    if (!input) {
      throw 'Expected search input';
    }

    ISearchUI.instance_ = new ISearchUI(input);
    input.focus();
    input.select();
    return ISearchUI.instance_;
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
            () => chrome.extension.getBackgroundPage()
                      .panelBackground.setRangeToISearchNode());
        PanelInterface.instance.closeMenusAndRestoreFocus();
        return false;
      default:
        return false;
    }
    chrome.extension.getBackgroundPage().panelBackground.incrementalSearch(
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
    chrome.extension.getBackgroundPage().panelBackground.incrementalSearch(
        searchStr, this.dir_);
    return true;
  }

  /** @override */
  onSearchReachedBoundary(boundaryNode) {
    this.output_(boundaryNode);
    ChromeVox.earcons.playEarcon(Earcon.WRAP);
  }

  /** @override */
  onSearchResultChanged(node, start, end) {
    this.output_(node, start, end);
  }

  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_start
   * @param {number=} opt_end
   * @private
   */
  output_(node, opt_start, opt_end) {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    const o = new Output();
    if (opt_start && opt_end) {
      o.withString([
        node.name.substr(0, opt_start),
        node.name.substr(opt_start, opt_end - opt_start),
        node.name.substr(opt_end)
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          cursors.Range.fromNode(node), null, OutputEventType.NAVIGATE);
    }
    o.go();

    this.background_.setCurrentRange(cursors.Range.fromNode(node));
  }

  /** Unregisters event handlers. */
  destroy() {
    chrome.extension.getBackgroundPage().panelBackground.destroyISearch();
    const input = this.input_;
    this.input_ = null;
    input.removeEventListener('keydown', this.onKeyDown, true);
    input.removeEventListener('textInput', this.onTextInput, false);
  }
}
