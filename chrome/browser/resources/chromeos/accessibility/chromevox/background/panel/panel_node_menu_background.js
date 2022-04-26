// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Calculates the menu items for the node menus in the ChromeVox
 * panel.
 */

export class PanelNodeMenuBackground {
  /**
   * @param {!AutomationPredicate.Unary} predicate
   * @param {boolean} isActiveMenu
   * @private
   */
  constructor(predicate, isActiveMenu) {
    /** @private {chrome.automation.AutomationNode} */
    this.node_ = ChromeVoxState.instance.currentRange.start;
    /** @private {AutomationTreeWalker|undefined} */
    this.walker_;
    /** @private {number} */
    this.nodeCount_ = 0;
    /** @private {!AutomationPredicate.Unary} */
    this.predicate_ = predicate;
    /** @private {boolean} */
    this.isActiveMenu_ = isActiveMenu;
    /** @private {boolean} */
    this.hasMenuItems_ = false;

    /** @private {!Array<!function()>} */
    this.menuActionCallbacks_ = [];
    /** @private {number} */
    this.menuId_ = PanelNodeMenuBackground.instances_.length;
    PanelNodeMenuBackground.instances_.push(this);

    if (this.isActiveMenu_) {
      // Put this at the front of the queue so it is calculated first.
      PanelNodeMenuBackground.populateQueue_.unshift(this);
    } else {
      PanelNodeMenuBackground.populateQueue_.push(this);
    }
  }

  /**
   * @param {string=} opt_activateMenuTitle
   * @return {!Array<!PanelNodeMenuData>}
   */
  static createAllPanelNodeMenuData(opt_activateMenuTitle) {
    const dataArray = [];
    for (const mapping of PanelNodeMenuBackground.roleListMenuMapping) {
      const isActiveMenu = mapping.menuTitle === opt_activateMenuTitle;
      dataArray.push(PanelNodeMenuBackground.createData_(
          mapping.menuTitle, mapping.predicate, isActiveMenu));
    }

    // Start populating node menus after we return.
    setTimeout(PanelNodeMenuBackground.populateAll_, 0);
    return dataArray;
  }

  /**
   * @param {number} menuId
   * @param {number} callbackId
   */
  static performMenuActionCallback(menuId, callbackId) {
    if (menuId >= PanelNodeMenuBackground.instances_.length || menuId < 0) {
      throw 'Error: Invalid menu ID provided to performMenuActionCallback';
    }
    const instance = PanelNodeMenuBackground.instances_[menuId];

    if (callbackId >= instance.menuActionCallbacks_.length || callbackId < 0) {
      return;
    }

    instance.menuActionCallbacks_[callbackId]();
  }

  /**
   * @param {string} menuTitle
   * @param {!AutomationPredicate.Unary} predicate
   * @param {boolean} isActiveMenu
   * @return {!PanelNodeMenuData}
   * @private
   */
  static createData_(menuTitle, predicate, isActiveMenu) {
    return (new PanelNodeMenuBackground(predicate, isActiveMenu))
        .getData_(menuTitle);
  }

  static populateAll_() {
    for (const instance of PanelNodeMenuBackground.populateQueue_) {
      instance.populate_();
    }
  }

  /**
   * @param {string} menuTitle
   * @private
   */
  getData_(menuTitle) {
    return new PanelNodeMenuData(menuTitle, this.menuId_);
  }


  /**
   * Create the AutomationTreeWalker and kick off the search to find
   * nodes that match the predicate for this menu.
   * @private
   */
  populate_() {
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
      }
    });
    this.findMoreNodes_();
  }

  /**
   * Iterate over nodes from the tree walker. If a node matches the
   * predicate, add an item to the menu data.
   *
   * Unless |this.isActiveMenu_| is true, after MAX_NODES_BEFORE_YIELDING nodes
   * have been scanned, call setTimeout to defer searching. This frees
   * up the main event loop to keep ChromeVox responsive, otherwise it basically
   * freezes up until all of the nodes have been found.
   * @private
   */
  findMoreNodes_() {
    let menuItems = [];
    while (this.walker_.next().node) {
      const node = this.walker_.node;
      if (this.predicate_(node)) {
        const isActive = this.isActiveMenu_ && node === this.node_;

        const output = new Output();
        const range = cursors.Range.fromNode(node);
        output.withoutHints();
        output.withSpeech(range, range, OutputEventType.NAVIGATE);
        const label = output.toString();

        const indexInArray = this.menuActionCallbacks_.length;
        this.menuActionCallbacks_.push(
            () => ChromeVoxState.instance.navigateToRange(
                cursors.Range.fromNode(node)));

        menuItems.push(
            new PanelNodeMenuItemData(label, indexInArray, isActive));
      }

      if (menuItems.length >= PanelNodeMenuBackground.BATCH_SIZE) {
        this.sendMenuItemData_(menuItems);
        menuItems = [];
      }

      this.nodeCount_++;
      // If this is not the active menu, yield periodically to avoid blocking
      // the user.
      if (!this.isActiveMenu_ &&
          this.nodeCount_ >=
              PanelNodeMenuBackground.MAX_NODES_BEFORE_YIELDING) {
        this.nodeCount_ = 0;

        if (menuItems.length) {
          this.sendMenuItemData_(menuItems);
          menuItems = [];
        }

        window.setTimeout(() => this.findMoreNodes_(), 0);
        return;
      }
    }

    if (menuItems.length) {
      this.sendMenuItemData_(menuItems);
    }
    this.finish_();
  }

  /**
   * @param {!Array<!PanelNodeMenuItemData>} menuItems
   * @private
   */
  async sendMenuItemData_(menuItems) {
    this.hasMenuItems_ = true;
    await PanelBridge.addPanelNodeMenuItems(this.menuId_, menuItems);
  }

  /** @private */
  finish_() {
    if (!this.hasMenuItems_) {
      this.sendMenuItemData_([new PanelNodeMenuItemData(
          Msgs.getMsg('panel_menu_item_none'), -1, false)]);
    }
  }
}

/**
 * The number of matching nodes to find before sending the data to the panel UI.
 * @const {number}
 */
PanelNodeMenuBackground.BATCH_SIZE = 5;

/**
 * The number of nodes to search before yielding to other tasks.
 * @const {number}
 */
PanelNodeMenuBackground.MAX_NODES_BEFORE_YIELDING = 100;

PanelNodeMenuBackground.roleListMenuMapping = [
  {menuTitle: 'role_heading', predicate: AutomationPredicate.heading},
  {menuTitle: 'role_landmark', predicate: AutomationPredicate.landmark},
  {menuTitle: 'role_link', predicate: AutomationPredicate.link}, {
    menuTitle: 'panel_menu_form_controls',
    predicate: AutomationPredicate.formField
  },
  {menuTitle: 'role_table', predicate: AutomationPredicate.table}
];

/** @private {!Array<!PanelNodeMenuBackground>} */
PanelNodeMenuBackground.instances_ = [];
/** @private {!Array<!PanelNodeMenuBackground>} */
PanelNodeMenuBackground.populateQueue_ = [];

BridgeHelper.registerHandler(
    /* target= */ 'PanelNodeMenuBackground', 'createAllPanelNodeMenuData',
    (opt_activateMenuTitle) =>
        PanelNodeMenuBackground.createAllPanelNodeMenuData(
            opt_activateMenuTitle));
BridgeHelper.registerHandler(
    /* target= */ 'PanelNodeMenuBackground', 'performMenuActionCallback',
    ({menuId, callbackId}) =>
        PanelNodeMenuBackground.performMenuActionCallback(menuId, callbackId));
