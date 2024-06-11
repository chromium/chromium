// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Calculates the menu items for the node menus in the ChromeVox
 * panel.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {BridgeCallbackId} from '/common/bridge_callback_manager.js';
import {constants} from '/common/constants.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import {AutomationTreeWalker} from '/common/tree_walker.js';

import {BridgeContext} from '../../common/bridge_constants.js';
import {Msgs} from '../../common/msgs.js';
import {PanelBridge} from '../../common/panel_bridge.js';
import {PanelNodeMenuData, PanelNodeMenuId, PanelNodeMenuItemData} from '../../common/panel_menu_data.js';
import {ChromeVoxRange} from '../chromevox_range.js';
import {Output} from '../output/output.js';
import {OutputCustomEvent} from '../output/output_types.js';

type AutomationNode = chrome.automation.AutomationNode;

export class PanelNodeMenuBackground {
  private node_: AutomationNode;
  private pred_: AutomationPredicate.Unary;
  private menuId_: PanelNodeMenuId;
  private isActivated_: boolean;
  private walker_?: AutomationTreeWalker;
  private nodeCount_ = 0;
  private isEmpty_ = true;
  private onFinish_?: VoidFunction;
  private finishPromise_: Promise<void>;


  /**
   * @param node ChromeVox's current position.
   * @param isActivated Whether the menu was explicitly activated.
   *     If false, the menu is populated asynchronously by posting a task
   *     after searching each chunk of nodes.
   */
  constructor(
      menuData: PanelNodeMenuData, node: AutomationNode, isActivated: boolean) {
    this.node_ = node;
    this.pred_ = menuData.predicate;
    this.menuId_ = menuData.menuId;
    this.isActivated_ = isActivated;
    this.finishPromise_ = new Promise(resolve => this.onFinish_ = resolve);
  }

  waitForFinish(): Promise<void> {
    return this.finishPromise_;
  }

  /**
   * Create the AutomationTreeWalker and kick off the search to find
   * nodes that match the predicate for this menu.
   */
  populate(): void {
    if (!this.node_) {
      this.finish_();
      return;
    }

    const root = AutomationUtil.getTopLevelRoot(this.node_);
    if (!root) {
      this.finish_();
      return;
    }

    this.walker_ = new AutomationTreeWalker(root, constants.Dir.FORWARD, {
      visit(node) {
        return !AutomationPredicate.shouldIgnoreNode(node);
      },
    });
    this.nodeCount_ = 0;
    this.findMoreNodes_();
  }

  /**
   * Iterate over nodes from the tree walker. If a node matches the
   * predicate, add an item to the menu.
   *
   * Unless |this.isActivated_| is true, then after MAX_NODES_BEFORE_ASYNC nodes
   * have been scanned, call setTimeout to defer searching. This frees up the
   * main event loop to keep the panel menu responsive, otherwise it basically
   * freezes up until all of the nodes have been found.
   */
  private findMoreNodes_(): void {
    // TODO(b/314203187): Not null asserted, check that this is correct.
    while (this.walker_!.next().node) {
      const node = this.walker_!.node;
      if (this.pred_(node!)) {
        this.isEmpty_ = false;
        const output = new Output();
        const range = CursorRange.fromNode(node!);
        output.withoutHints();
        output.withSpeech(range, range, OutputCustomEvent.NAVIGATE);
        const title = output.toString();

        const callbackId = new BridgeCallbackId(
            BridgeContext.BACKGROUND,
            () => ChromeVoxRange.navigateTo(CursorRange.fromNode(node!)));
        const isActive = node === this.node_ && this.isActivated_;
        const menuId = this.menuId_;
        this.addMenuItemFromData_({title, callbackId, isActive, menuId});
      }

      if (!this.isActivated_) {
        this.nodeCount_++;
        if (this.nodeCount_ >= MAX_NODES_BEFORE_ASYNC) {
          this.nodeCount_ = 0;
          setTimeout(() => this.findMoreNodes_(), 0);
          return;
        }
      }
    }
    this.finish_();
  }

  /**
   * Called when we've finished searching for nodes. If no matches were
   * found, adds an item to the menu indicating none were found.
   */
  private finish_(): void {
    if (this.isEmpty_) {
      this.addMenuItemFromData_({
        title: Msgs.getMsg('panel_menu_item_none'),
        callbackId: null,
        isActive: false,
        menuId: this.menuId_,
      });
    }
    // TODO(b/314203187): Not null asserted, check that this is correct.
    this.onFinish_!();
  }

  private async addMenuItemFromData_(
      itemData: PanelNodeMenuItemData): Promise<void> {
    await PanelBridge.addMenuItem(itemData);
  }
}

// Local to module.

/** The number of nodes to search before posting a task to finish searching. */
const MAX_NODES_BEFORE_ASYNC = 100;

TestImportManager.exportForTesting(PanelNodeMenuBackground);
