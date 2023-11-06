// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {addNode, addNodeWithText, setNodeDisplay, setNodePosition} from './util.js';
import {View} from './view.js';

const TAB_LIST_ID = 'tab-list';

/** @type {?TabSwitcherView} */
let instance = null;

/**
 * Controller and View for switching between tabs.
 *
 * The View part of TabSwitcherView displays the contents of the currently
 * selected tab (only one tab can be active at a time).
 *
 * The controller part of TabSwitcherView hooks up a dropdown menu (i.e. HTML
 * SELECT) to control switching between tabs.
 */
export class TabSwitcherView extends View {
  constructor() {
    super();

    this.tabIdToView_ = {};
    this.tabIdToLink_ = {};
    // Map from tab id to the views link visiblity.
    this.tabIdsLinkVisibility_ = new Map();
    this.activeTabId_ = null;
    this.onTabSwitched_ = null;

    // The ideal width of the tab list.  If width is reduced below this, the
    // tab list will be shrunk, but it will be returned to this width once it
    // can be.
    this.tabListWidth_ = $(TAB_LIST_ID).offsetWidth;
  }

  /**
   * @param {!Function} onTabSwitched Callback to run when the
   *                    active tab changes. Called as
   *                    onTabSwitched(oldTabId, newTabId).
   */
  setOnTabSwitched(onTabSwitched) {
    this.onTabSwitched_ = onTabSwitched;
  }

  // ---------------------------------------------
  // Override methods in View
  // ---------------------------------------------

  setGeometry(left, top, width, height) {
    super.setGeometry(this, left, top, width, height);

    const tabListNode = $(TAB_LIST_ID);

    // Set position of the tab list.  Can't use DivView because DivView sets
    // a fixed width at creation time, and need to set the width of the tab
    // list only after its been populated.
    let tabListWidth = this.tabListWidth_;
    if (tabListWidth > width) {
      tabListWidth = width;
    }
    tabListNode.style.position = 'absolute';
    setNodePosition(tabListNode, left, top, tabListWidth, height);

    // Position each of the tab's content areas.
    for (const tabId in this.tabIdToView_) {
      const view = this.tabIdToView_[tabId];
      view.setGeometry(left + tabListWidth, top, width - tabListWidth, height);
    }
  }

  show(isVisible) {
    super.show(isVisible);
    const activeView = this.getActiveTabView();
    if (activeView) {
      activeView.show(isVisible);
    }
  }

  // ---------------------------------------------

  /**
   * Adds a new tab (initially hidden).  To ensure correct tab list sizing,
   * may only be called before first layout.
   *
   * @param {string} tabId The ID to refer to the tab by.
   * @param {!View} view The tab's actual contents.
   * @param {string} name The name for the menu item that selects the tab.
   */
  addTab(tabId, view, name, hash) {
    if (!tabId) {
      throw Error('Must specify a non-false tabId');
    }

    this.tabIdToView_[tabId] = view;
    this.tabIdsLinkVisibility_.set(tabId, true);

    const node = addNodeWithText($(TAB_LIST_ID), 'a', name);
    node.href = hash;
    this.tabIdToLink_[tabId] = node;
    addNode($(TAB_LIST_ID), 'br');

    // Tab content views start off hidden.
    view.show(false);

    this.tabListWidth_ = $(TAB_LIST_ID).offsetWidth;
  }

  getAllTabViews() {
    return this.tabIdToView_;
  }

  getTabView(tabId) {
    return this.tabIdToView_[tabId];
  }

  getActiveTabView() {
    return this.tabIdToView_[this.activeTabId_];
  }

  getActiveTabId() {
    return this.activeTabId_;
  }

  /**
   * Changes the currently active tab to |tabId|. This has several effects:
   *   (1) Replace the tab contents view with that of the new tab.
   *   (2) Update the dropdown menu's current selection.
   *   (3) Invoke the optional onTabSwitched callback.
   */
  switchToTab(tabId) {
    const newView = this.getTabView(tabId);

    if (!newView) {
      throw Error('Invalid tabId');
    }

    const oldTabId = this.activeTabId_;
    this.activeTabId_ = tabId;

    if (oldTabId) {
      this.tabIdToLink_[oldTabId].classList.remove('selected');
      // Hide the previously visible tab contents.
      this.getTabView(oldTabId).show(false);
    }

    this.tabIdToLink_[tabId].classList.add('selected');

    newView.show(this.isVisible());

    if (this.onTabSwitched_) {
      this.onTabSwitched_(oldTabId, tabId);
    }
  }

  static getInstance() {
    return instance || (instance = new TabSwitcherView());
  }
}
