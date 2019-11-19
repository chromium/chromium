// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A drop-down menu in the ChromeVox panel.
 */

goog.provide('PanelMenu');
goog.provide('PanelNodeMenu');

goog.require('AutomationTreeWalker');
goog.require('Output');
goog.require('PanelMenuItem');
goog.require('constants');
goog.require('cursors.Range');

/**
 * @param {string} menuMsg The msg id of the menu.
 * @constructor
 */
PanelMenu = function(menuMsg) {
  /** @type {string} */
  this.menuMsg = menuMsg;
  // The item in the menu bar containing the menu's title.
  this.menuBarItemElement = document.createElement('div');
  this.menuBarItemElement.className = 'menu-bar-item';
  this.menuBarItemElement.setAttribute('role', 'menu');
  var menuTitle = Msgs.getMsg(menuMsg);
  this.menuBarItemElement.textContent = menuTitle;

  // The container for the menu. This part is fixed and scrolls its
  // contents if necessary.
  this.menuContainerElement = document.createElement('div');
  this.menuContainerElement.className = 'menu-container';
  this.menuContainerElement.style.visibility = 'hidden';

  // The menu itself. It contains all of the items, and it scrolls within
  // its container.
  this.menuElement = document.createElement('table');
  this.menuElement.className = 'menu';
  this.menuElement.setAttribute('role', 'menu');
  this.menuElement.setAttribute('aria-label', menuTitle);
  this.menuContainerElement.appendChild(this.menuElement);

  /**
   * The items in the menu.
   * @type {Array<PanelMenuItem>}
   * @private
   */
  this.items_ = [];

  /**
   * The return value from window.setTimeout for a function to update the
   * scroll bars after an item has been added to a menu. Used so that we
   * don't re-layout too many times.
   * @type {?number}
   * @private
   */
  this.updateScrollbarsTimeout_ = null;

  /**
   * The current active menu item index, or -1 if none.
   * @type {number}
   * @private
   */
  this.activeIndex_ = -1;

  this.menuElement.addEventListener(
      'keypress', this.onKeyPress_.bind(this), true);
};

PanelMenu.prototype = {
  /**
   * @param {string} menuItemTitle The title of the menu item.
   * @param {string} menuItemShortcut The keystrokes to select this item.
   * @param {string} menuItemBraille
   * @param {string} gesture
   * @param {Function} callback The function to call if this item is selected.
   * @return {!PanelMenuItem} The menu item just created.
   */
  addMenuItem: function(
      menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback) {
    var menuItem = new PanelMenuItem(
        menuItemTitle, menuItemShortcut, menuItemBraille, gesture, callback);
    this.items_.push(menuItem);
    this.menuElement.appendChild(menuItem.element);

    // Sync the active index with focus.
    menuItem.element.addEventListener(
        'focus', (function(index, event) {
                   this.activeIndex_ = index;
                 }).bind(this, this.items_.length - 1),
        false);

    // Update the container height, adding a scroll bar if necessary - but
    // to avoid excessive layout, schedule this once per batch of adding
    // menu items rather than after each add.
    if (!this.updateScrollbarsTimeout_) {
      this.updateScrollbarsTimeout_ = window.setTimeout(
          (function() {
            var menuBounds = this.menuElement.getBoundingClientRect();
            var maxHeight = window.innerHeight - menuBounds.top;
            this.menuContainerElement.style.maxHeight = maxHeight + 'px';
            this.updateScrollbarsTimeout_ = null;
          }).bind(this),
          0);
    }

    return menuItem;
  },

  /**
   * Activate this menu, which means showing it and positioning it on the
   * screen underneath its title in the menu bar.
   */
  activate: function() {
    this.menuContainerElement.style.visibility = 'visible';
    this.menuContainerElement.style.opacity = 1;
    this.menuBarItemElement.classList.add('active');
    var barBounds =
        this.menuBarItemElement.parentElement.getBoundingClientRect();
    var titleBounds = this.menuBarItemElement.getBoundingClientRect();
    var menuBounds = this.menuElement.getBoundingClientRect();

    this.menuElement.style.minWidth = titleBounds.width + 'px';
    this.menuContainerElement.style.minWidth = titleBounds.width + 'px';
    if (titleBounds.left + menuBounds.width < barBounds.width) {
      this.menuContainerElement.style.left = titleBounds.left + 'px';
    } else {
      this.menuContainerElement.style.left =
          (titleBounds.right - menuBounds.width) + 'px';
    }

    // Make the first item active.
    this.activateItem(0);
  },

  /**
   * Hide this menu. Make it invisible first to minimize spurious
   * accessibility events before the next menu activates.
   */
  deactivate: function() {
    this.menuContainerElement.style.opacity = 0.001;
    this.menuBarItemElement.classList.remove('active');
    this.activeIndex_ = -1;

    window.setTimeout(
        (function() {
          this.menuContainerElement.style.visibility = 'hidden';
        }).bind(this),
        0);
  },

  /**
   * Make a specific menu item index active.
   * @param {number} itemIndex The index of the menu item.
   */
  activateItem: function(itemIndex) {
    this.activeIndex_ = itemIndex;
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      this.items_[this.activeIndex_].element.focus();
    }
  },

  /**
   * Advanced the active menu item index by a given number.
   * @param {number} delta The number to add to the active menu item index.
   */
  advanceItemBy: function(delta) {
    if (this.activeIndex_ >= 0) {
      this.activeIndex_ += delta;
      this.activeIndex_ =
          (this.activeIndex_ + this.items_.length) % this.items_.length;
    } else {
      if (delta >= 0) {
        this.activeIndex_ = 0;
      } else {
        this.activeIndex_ = this.items_.length - 1;
      }
    }

    this.items_[this.activeIndex_].element.focus();
  },

  /**
   * Sets the active menu item index to be 0.
   */
  scrollToTop: function() {
    this.activeIndex_ = 0;
    this.items_[this.activeIndex_].element.focus();
  },

  /**
   * Sets the active menu item index to be the last index.
   */
  scrollToBottom: function() {
    this.activeIndex_ = this.items_.length - 1;
    this.items_[this.activeIndex_].element.focus();
  },

  /**
   * Get the callback for the active menu item.
   * @return {Function} The callback.
   */
  getCallbackForCurrentItem: function() {
    if (this.activeIndex_ >= 0 && this.activeIndex_ < this.items_.length) {
      return this.items_[this.activeIndex_].callback;
    }
    return null;
  },

  /**
   * Get the callback for a menu item given its DOM element.
   * @param {Element} element The DOM element.
   * @return {Function} The callback.
   */
  getCallbackForElement: function(element) {
    for (var i = 0; i < this.items_.length; i++) {
      if (element == this.items_[i].element) {
        return this.items_[i].callback;
      }
    }
    return null;
  },

  /**
   * Handles key presses for first letter accelerators.
   */
  onKeyPress_: function(evt) {
    if (!this.items_.length) {
      return;
    }

    var query = String.fromCharCode(evt.charCode).toLowerCase();
    for (var i = this.activeIndex_ + 1; i != this.activeIndex_;
         i = (i + 1) % this.items_.length) {
      if (this.items_[i].text.toLowerCase().indexOf(query) == 0) {
        this.activateItem(i);
        break;
      }
    }
  }
};

/**
 * @param {string} menuMsg The msg id of the menu.
 * @param {chrome.automation.AutomationNode} node ChromeVox's current position.
 * @param {AutomationPredicate.Unary} pred Filter to use on the document.
 * @param {boolean} async If true, populates the menu asynchronously by
 *     posting a task after searching each chunk of nodes.
 * @extends {PanelMenu}
 * @constructor
 */
PanelNodeMenu = function(menuMsg, node, pred, async) {
  PanelMenu.call(this, menuMsg);
  this.node_ = node;
  this.pred_ = pred;
  this.async_ = async;
  this.populate_();
};

/**
 * The number of nodes to search before posting a task to finish
 * searching.
 * @const {number}
 */
PanelNodeMenu.MAX_NODES_BEFORE_ASYNC = 100;

PanelNodeMenu.prototype = {
  __proto__: PanelMenu.prototype,

  /** @override */
  activate: function() {
    var activeItem = this.activeIndex_;
    PanelMenu.prototype.activate.call(this);
    this.activateItem(activeItem);
  },

  /**
   * Create the AutomationTreeWalker and kick off the search to find
   * nodes that match the predicate for this menu.
   * @private
   */
  populate_: function() {
    if (!this.node_) {
      this.finish_();
      return;
    }

    var root = AutomationUtil.getTopLevelRoot(this.node_);
    if (!root) {
      this.finish_();
      return;
    }

    this.walker_ = new AutomationTreeWalker(root, constants.Dir.FORWARD, {
      visit: function(node) {
        return !AutomationPredicate.shouldIgnoreNode(node);
      }
    });
    this.nodeCount_ = 0;
    this.selectNext_ = false;
    this.findMoreNodes_();
  },

  /**
   * Iterate over nodes from the tree walker. If a node matches the
   * predicate, add an item to the menu.
   *
   * If |this.async_| is true, then after MAX_NODES_BEFORE_ASYNC nodes
   * have been scanned, call setTimeout to defer searching. This frees
   * up the main event loop to keep the panel menu responsive, otherwise
   * it basically freezes up until all of the nodes have been found.
   * @private
   */
  findMoreNodes_: function() {
    while (this.walker_.next().node) {
      var node = this.walker_.node;
      if (node == this.node_) {
        this.selectNext_ = true;
      }
      if (this.pred_(node)) {
        var output = new Output();
        var range = cursors.Range.fromNode(node);
        output.withoutHints();
        output.withSpeech(range, range, Output.EventType.NAVIGATE);
        var label = output.toString();
        this.addMenuItem(label, '', '', '', (function() {
                           var savedNode = node;
                           return function() {
                             chrome.extension.getBackgroundPage()
                                 .ChromeVoxState.instance['navigateToRange'](
                                     cursors.Range.fromNode(savedNode));
                           };
                         }()));

        if (this.selectNext_) {
          this.activateItem(this.items_.length - 1);
          this.selectNext_ = false;
        }
      }

      if (this.async_) {
        this.nodeCount_++;
        if (this.nodeCount_ >= PanelNodeMenu.MAX_NODES_BEFORE_ASYNC) {
          this.nodeCount_ = 0;
          window.setTimeout(this.findMoreNodes_.bind(this), 0);
          return;
        }
      }
    }
    this.finish_();
  },

  /**
   * Called when we've finished searching for nodes. If no matches were
   * found, adds an item to the menu indicating none were found.
   * @private
   */
  finish_: function() {
    if (!this.items_.length) {
      this.addMenuItem(
          Msgs.getMsg('panel_menu_item_none'), '', '', '', function() {});
      this.activateItem(0);
    }
  }
};
