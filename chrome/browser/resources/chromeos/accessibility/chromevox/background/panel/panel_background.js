// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {CursorRange} from '../../../common/cursors/range.js';
import {PanelBridge} from '../../common/panel_bridge.js';
import {ChromeVoxState, ChromeVoxStateObserver} from '../chromevox_state.js';
import {Output} from '../output/output.js';
import {OutputEventType} from '../output/output_types.js';

import {ISearch} from './i_search.js';
import {ISearchHandler} from './i_search_handler.js';
import {PanelNodeMenuBackground} from './panel_node_menu_background.js';
import {PanelTabMenuBackground} from './panel_tab_menu_background.js';

const AutomationNode = chrome.automation.AutomationNode;
const Constants = BridgeConstants.PanelBackground;

/** @implements {ISearchHandler} */
export class PanelBackground {
  /** @private */
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
    /** @private {AutomationNode} */
    this.savedNode_;
  }

  static init() {
    if (PanelBackground.instance) {
      throw 'Trying to create two copies of singleton PanelBackground';
    }
    PanelBackground.instance = new PanelBackground();
    // Temporarily expose the panel background instance on the window object so
    // it can be accessed from other renderers as we transition the logic to the
    // background context.
    window.panelBackground = PanelBackground.instance;

    PanelBackground.stateObserver_ = new PanelStateObserver();
    ChromeVoxState.addObserver(PanelBackground.stateObserver_);

    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.CLEAR_SAVED_NODE,
        () => PanelBackground.instance.clearSavedNode_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.CREATE_ALL_NODE_MENU_BACKGROUNDS,
        opt_activateMenuTitle =>
            PanelBackground.instance.createAllNodeMenuBackgrounds_(
                opt_activateMenuTitle));
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.CREATE_NEW_I_SEARCH,
        () => PanelBackground.instance.createNewISearch_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.DESTROY_I_SEARCH,
        () => PanelBackground.instance.destroyISearch_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.FOCUS_TAB,
        ({windowId, tabId}) =>
            PanelTabMenuBackground.focusTab(windowId, tabId));
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.GET_ACTIONS_FOR_CURRENT_NODE,
        () => PanelBackground.instance.getActionsForCurrentNode_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.GET_TAB_MENU_DATA,
        () => PanelTabMenuBackground.getTabMenuData());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.INCREMENTAL_SEARCH,
        ({searchStr, dir, opt_nextObject}) =>
            PanelBackground.instance.incrementalSearch_(
                searchStr, dir, opt_nextObject));
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.NODE_MENU_CALLBACK,
        callbackNodeIndex =>
            PanelNodeMenuBackground.focusNodeCallback(callbackNodeIndex));
    BridgeHelper.registerHandler(
        Constants.TARGET,
        Constants.Action.PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE,
        actionId => PanelBackground.instance.performCustomActionOnCurrentNode_(
            actionId));
    BridgeHelper.registerHandler(
        Constants.TARGET,
        Constants.Action.PERFORM_STANDARD_ACTION_ON_CURRENT_NODE,
        action => PanelBackground.instance.performStandardActionOnCurrentNode_(
            action));
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.SAVE_CURRENT_NODE,
        () => PanelBackground.instance.saveCurrentNode_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.SET_RANGE_TO_I_SEARCH_NODE,
        () => PanelBackground.instance.setRangeToISearchNode_());
    BridgeHelper.registerHandler(
        Constants.TARGET, Constants.Action.WAIT_FOR_PANEL_COLLAPSE,
        () => PanelBackground.instance.waitForPanelCollapse_());
  }

  /** @private */
  clearSavedNode_() {
    this.savedNode_ = null;
  }

  /**
   * @param {string=} opt_activateMenuTitleId Optional string specifying the
   *     activated menu.
   * @private
   */
  createAllNodeMenuBackgrounds_(opt_activateMenuTitleId) {
    for (const data of ALL_NODE_MENU_DATA) {
      const isActivatedMenu = opt_activateMenuTitleId === data.titleId;
      const menuBackground =
          new PanelNodeMenuBackground(data, this.savedNode_, isActivatedMenu);
      menuBackground.populate();
    }
  }

  /**
   * Creates a new ISearch object, ready to search starting from the current
   * ChromeVox focus.
   * @private
   */
  createNewISearch_() {
    if (this.iSearch_) {
      this.iSearch_.clear();
    }
    // TODO(accessibility): not sure if this actually works anymore since all
    // the refactoring.
    if (!ChromeVoxState.instance.currentRange ||
        !ChromeVoxState.instance.currentRange.start) {
      return;
    }
    this.iSearch_ = new ISearch(ChromeVoxState.instance.currentRange.start);
    this.iSearch_.handler = this;
  }

  /**
   * Destroy the ISearch object so it can be garbage collected.
   * @private
   */
  destroyISearch_() {
    this.iSearch_.handler = null;
    this.iSearch_ = null;
  }

  /**
   * @return {{
   *     standardActions: !Array<!chrome.automation.ActionType>,
   *     customActions: !Array<!chrome.automation.CustomAction>
   * }}
   * @private
   */
  getActionsForCurrentNode_() {
    const result = {
      standardActions: [],
      customActions: [],
    };
    if (!this.savedNode_) {
      return result;
    }
    if (this.savedNode_.standardActions) {
      result.standardActions = this.savedNode_.standardActions;
    }
    if (this.savedNode_.customActions) {
      result.customActions = this.savedNode_.customActions;
    }
    return result;
  }

  /**
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   * @private
   */
  incrementalSearch_(searchStr, dir, opt_nextObject) {
    if (!this.iSearch_) {
      console.error(
          'Trying to incrementally search when no ISearch has been created');
      return;
    }

    this.iSearch_.search(searchStr, dir, opt_nextObject);
  }

  /**
   * @param {number} actionId
   * @private
   */
  performCustomActionOnCurrentNode_(actionId) {
    if (this.savedNode_) {
      this.savedNode_.performCustomAction(actionId);
    }
  }

  /**
   * @param {!chrome.automation.ActionType} action
   * @private
   */
  performStandardActionOnCurrentNode_(action) {
    if (this.savedNode_) {
      this.savedNode_.performStandardAction(action);
    }
  }

  /**
   * Sets the current ChromeVox focus to the current ISearch node.
   * @private
   */
  setRangeToISearchNode_() {
    if (!this.iSearch_) {
      console.error(
          'Setting range to ISearch node when no ISearch in progress');
      return;
    }

    const node = this.iSearch_.cursor.node;
    if (!node) {
      return;
    }
    ChromeVoxState.instance.navigateToRange(CursorRange.fromNode(node));
  }

  /** @override */
  onSearchReachedBoundary(boundaryNode) {
    this.iSearchOutput_(boundaryNode);
    ChromeVox.earcons.playEarcon(Earcon.WRAP);
  }

  /** @override */
  onSearchResultChanged(node, start, end) {
    this.iSearchOutput_(node, start, end);
  }

  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_start
   * @param {number=} opt_end
   * @private
   */
  iSearchOutput_(node, opt_start, opt_end) {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    const o = new Output();
    if (opt_start && opt_end) {
      o.withString([
        node.name.substr(0, opt_start),
        node.name.substr(opt_start, opt_end - opt_start),
        node.name.substr(opt_end),
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          CursorRange.fromNode(node), null, OutputEventType.NAVIGATE);
    }
    o.go();

    ChromeVoxState.instance.setCurrentRange(CursorRange.fromNode(node));
  }

  /** @private */
  saveCurrentNode_() {
    this.savedNode_ = ChromeVoxState.instance.currentRange.start.node;
  }

  /**
   * Listens for focus events, and returns once the target is not the panel.
   * @private
   */
  async waitForPanelCollapse_() {
    return new Promise(async resolve => {
      const desktop =
          await new Promise((resolve) => chrome.automation.getDesktop(resolve));
      // Watch for a focus event outside the panel.
      const onFocus = event => {
        if (event.target.docUrl &&
            event.target.docUrl.includes('chromevox/panel')) {
          return;
        }

        desktop.removeEventListener(
            chrome.automation.EventType.FOCUS, onFocus, true);

        // Clears focus on the page by focusing the root explicitly. This makes
        // sure we don't get future focus events as a result of giving this
        // entire page focus (which would interfere with our desired range).
        if (event.target.root) {
          event.target.root.focus();
        }

        resolve();
      };

      desktop.addEventListener(
          chrome.automation.EventType.FOCUS, onFocus, true);
    });
  }
}

/** @type {PanelBackground} */
PanelBackground.instance;

/** @private {PanelStateObserver} */
PanelBackground.stateObserver_;

/** @implements {ChromeVoxStateObserver} */
class PanelStateObserver {
  /** @override */
  onCurrentRangeChanged(range, opt_fromEditing) {
    PanelBridge.onCurrentRangeChanged();
  }
}
