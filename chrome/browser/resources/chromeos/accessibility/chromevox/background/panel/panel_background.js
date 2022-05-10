// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {ISearch} from '/chromevox/background/panel/i_search.js';
import {ISearchHandler} from '/chromevox/background/panel/i_search_handler.js';
import {PanelNodeMenuBackground} from '/chromevox/background/panel/panel_node_menu_background.js';
import {PanelTabMenuBackground} from '/chromevox/background/panel/panel_tab_menu_background.js';

const AutomationNode = chrome.automation.AutomationNode;

/** @implements {ISearchHandler} */
export class PanelBackground {
  /** @private */
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
    /** @private {AutomationNode} */
    this.nodeForActions_;
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

    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.CREATE_ALL_NODE_MENU_BACKGROUNDS,
        (opt_activateMenuTitle) =>
            PanelBackground.instance.createAllNodeMenuBackgrounds_(
                opt_activateMenuTitle));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.CREATE_NEW_I_SEARCH,
        () => PanelBackground.instance.createNewISearch_());
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.DESTROY_I_SEARCH,
        () => PanelBackground.instance.destroyISearch_());
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.FOCUS_TAB,
        ({windowId, tabId}) =>
            PanelTabMenuBackground.focusTab(windowId, tabId));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.GET_ACTIONS_FOR_CURRENT_NODE,
        () => PanelBackground.instance.getActionsForCurrentNode_());
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.GET_TAB_MENU_DATA,
        () => PanelTabMenuBackground.getTabMenuData());
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.INCREMENTAL_SEARCH,
        ({searchStr, dir, opt_nextObject}) =>
            PanelBackground.instance.incrementalSearch_(
                searchStr, dir, opt_nextObject));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.NODE_MENU_CALLBACK,
        (callbackNodeIndex) =>
            PanelNodeMenuBackground.focusNodeCallback(callbackNodeIndex));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE,
        (actionId) =>
            PanelBackground.instance.performCustomActionOnCurrentNode_(
                actionId));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND,
        BridgeAction.PERFORM_STANDARD_ACTION_ON_CURRENT_NODE,
        (action) =>
            PanelBackground.instance.performStandardActionOnCurrentNode_(
                action));
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.SET_RANGE_TO_I_SEARCH_NODE,
        () => PanelBackground.instance.setRangeToISearchNode_());
    BridgeHelper.registerHandler(
        BridgeTarget.PANEL_BACKGROUND, BridgeAction.WAIT_FOR_PANEL_COLLAPSE,
        () => PanelBackground.instance.waitForPanelCollapse_());
  }

  /**
   * @param {string=} opt_activateMenuTitleId Optional string specifying the
   *     activated menu.
   * @private
   */
  createAllNodeMenuBackgrounds_(opt_activateMenuTitleId) {
    const node = ChromeVoxState.instance.currentRange.start.node;
    for (const data of ALL_NODE_MENU_DATA) {
      const isActivatedMenu = opt_activateMenuTitleId === data.titleId;
      const menuBackground =
          new PanelNodeMenuBackground(data, node, isActivatedMenu);
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
    this.nodeForActions_ = ChromeVoxState.instance.currentRange.start;
    const result = {
      standardActions: [],
      customActions: [],
    };
    if (!this.nodeForActions_) {
      return result;
    }
    if (this.nodeForActions_.standardActions) {
      result.standardActions = this.nodeForActions_.standardActions;
    }
    if (this.nodeForActions_.customActions) {
      result.customActions = this.nodeForActions_.customActions;
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
    if (this.nodeForActions_) {
      this.nodeForActions_.performCustomAction(actionId);
    }
  }

  /**
   * @param {!chrome.automation.ActionType} action
   * @private
   */
  performStandardActionOnCurrentNode_(action) {
    if (this.nodeForActions_) {
      this.nodeForActions_.performStandardAction(action);
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
    ChromeVoxState.instance.navigateToRange(cursors.Range.fromNode(node));
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
        node.name.substr(opt_end)
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          cursors.Range.fromNode(node), null, OutputEventType.NAVIGATE);
    }
    o.go();

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));
  }

  /**
   * Listens for focus events, and returns once the target is not the panel.
   * @private
   */
  async waitForPanelCollapse_() {
    return new Promise(async resolve => {
      const desktop = await new Promise(chrome.automation.getDesktop);
      // Watch for a focus event outside the panel.
      const onFocus = (event) => {
        if (event.target.docUrl.contains('chromevox/panel')) {
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
