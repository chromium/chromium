// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {AsyncUtil} from '../../../common/async_util.js';
import {constants} from '../../../common/constants.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {BridgeConstants} from '../../common/bridge_constants.js';
import {BridgeHelper} from '../../common/bridge_helper.js';
import {EarconId} from '../../common/earcon_id.js';
import {PanelBridge} from '../../common/panel_bridge.js';
import {ALL_PANEL_MENU_NODE_DATA} from '../../common/panel_menu_data.js';
import {QueueMode} from '../../common/tts_types.js';
import {ChromeVox} from '../chromevox.js';
import {ChromeVoxRange, ChromeVoxRangeObserver} from '../chromevox_range.js';
import {ChromeVoxState} from '../chromevox_state.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';

import {ISearch} from './i_search.js';
import {ISearchHandler} from './i_search_handler.js';
import {PanelNodeMenuBackground} from './panel_node_menu_background.js';
import {PanelTabMenuBackground} from './panel_tab_menu_background.js';

const AutomationNode = chrome.automation.AutomationNode;
const TARGET = BridgeConstants.PanelBackground.TARGET;
const Action = BridgeConstants.PanelBackground.Action;

/** @implements {ISearchHandler} */
export class PanelBackground {
  /** @private */
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
    /** @private {AutomationNode} */
    this.savedNode_;
    /** @private {Promise} */
    this.resolvePanelCollapsed_;
  }

  static init() {
    if (PanelBackground.instance) {
      throw 'Trying to create two copies of singleton PanelBackground';
    }
    PanelBackground.instance = new PanelBackground();
    PanelBackground.rangeObserver_ = new PanelRangeObserver();
    ChromeVoxRange.addObserver(PanelBackground.rangeObserver_);

    BridgeHelper.registerHandler(
        TARGET, Action.CLEAR_SAVED_NODE,
        () => PanelBackground.instance.clearSavedNode_());
    BridgeHelper.registerHandler(
        TARGET, Action.CREATE_ALL_NODE_MENU_BACKGROUNDS,
        opt_activateMenuTitle =>
            PanelBackground.instance.createAllNodeMenuBackgrounds_(
                opt_activateMenuTitle));
    BridgeHelper.registerHandler(
        TARGET, Action.CREATE_NEW_I_SEARCH,
        () => PanelBackground.instance.createNewISearch_());
    BridgeHelper.registerHandler(
        TARGET, Action.DESTROY_I_SEARCH,
        () => PanelBackground.instance.destroyISearch_());
    BridgeHelper.registerHandler(
        TARGET, Action.FOCUS_TAB,
        (windowId, tabId) => PanelTabMenuBackground.focusTab(windowId, tabId));
    BridgeHelper.registerHandler(
        TARGET, Action.GET_ACTIONS_FOR_CURRENT_NODE,
        () => PanelBackground.instance.getActionsForCurrentNode_());
    BridgeHelper.registerHandler(
        TARGET, Action.GET_TAB_MENU_DATA,
        () => PanelTabMenuBackground.getTabMenuData());
    BridgeHelper.registerHandler(
        TARGET, Action.INCREMENTAL_SEARCH,
        (searchStr, dir, opt_nextObject) =>
            PanelBackground.instance.incrementalSearch_(
                searchStr, dir, opt_nextObject));
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_CUSTOM_ACTION_ON_CURRENT_NODE,
        actionId => PanelBackground.instance.performCustomActionOnCurrentNode_(
            actionId));
    BridgeHelper.registerHandler(
        TARGET, Action.PERFORM_STANDARD_ACTION_ON_CURRENT_NODE,
        action => PanelBackground.instance.performStandardActionOnCurrentNode_(
            action));
    BridgeHelper.registerHandler(
        TARGET, Action.SAVE_CURRENT_NODE,
        () => PanelBackground.instance.saveCurrentNode_());
    BridgeHelper.registerHandler(
        TARGET, Action.SET_PANEL_COLLAPSE_WATCHER,
        () => PanelBackground.instance.setPanelCollapseWatcher_());
    BridgeHelper.registerHandler(
        TARGET, Action.SET_RANGE_TO_I_SEARCH_NODE,
        () => PanelBackground.instance.setRangeToISearchNode_());
    BridgeHelper.registerHandler(
        TARGET, Action.WAIT_FOR_PANEL_COLLAPSE,
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
    if (!this.savedNode_) {
      return;
    }
    for (const data of ALL_PANEL_MENU_NODE_DATA) {
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
    if (!ChromeVoxRange.current || !ChromeVoxRange.current.start) {
      return;
    }
    this.iSearch_ = new ISearch(ChromeVoxRange.current.start);
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
    ChromeVox.earcons.playEarcon(EarconId.WRAP);
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
          CursorRange.fromNode(node), null, OutputCustomEvent.NAVIGATE);
    }
    o.go();

    ChromeVoxRange.set(CursorRange.fromNode(node));
  }

  /**
   * Adds an event listener to detect panel collapse.
   * @private
   */
  async setPanelCollapseWatcher_() {
    const desktop = await AsyncUtil.getDesktop();
    let notifyPanelCollapsed;
    this.resolvePanelCollapsed_ = new Promise(resolve => {
      notifyPanelCollapsed = resolve;
    });
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

      notifyPanelCollapsed();
    };

    desktop.addEventListener(chrome.automation.EventType.FOCUS, onFocus, true);
  }

  /** @private */
  saveCurrentNode_() {
    if (ChromeVoxRange.current) {
      this.savedNode_ = ChromeVoxRange.current.start.node;
    }
  }

  /**
   * Wait for the promise to notify panel collapse to resolved.
   * @private
   */
  async waitForPanelCollapse_() {
    return this.resolvePanelCollapsed_;
  }
}

/** @type {PanelBackground} */
PanelBackground.instance;

/** @private {PanelRangeObserver} */
PanelBackground.rangeObserver_;

/** @implements {ChromeVoxRangeObserver} */
class PanelRangeObserver {
  /** @override */
  onCurrentRangeChanged(range, opt_fromEditing) {
    PanelBridge.onCurrentRangeChanged();
  }
}
