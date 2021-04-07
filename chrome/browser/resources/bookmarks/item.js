// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './shared_style.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {selectItem} from './actions.js';
import {CommandManager} from './command_manager.js';
import {Command, MenuSource} from './constants.js';
import {StoreClient} from './store_client.js';
import {BookmarkNode} from './types.js';

Polymer({
  is: 'bookmarks-item',

  _template: html`{__html_template__}`,

  behaviors: [
    StoreClient,
  ],

  properties: {
    itemId: {
      type: String,
      observer: 'onItemIdChanged_',
    },

    ironListTabIndex: Number,

    /** @private {BookmarkNode} */
    item_: {
      type: Object,
      observer: 'onItemChanged_',
    },

    /** @private */
    isSelectedItem_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private */
    isMultiSelect_: Boolean,

    /** @private */
    isFolder_: Boolean,

    /** @private */
    lastTouchPoints_: Number,
  },

  observers: [
    'updateFavicon_(item_.url)',
  ],

  listeners: {
    'click': 'onClick_',
    'dblclick': 'onDblClick_',
    'contextmenu': 'onContextMenu_',
    'keydown': 'onKeydown_',
    'auxclick': 'onMiddleClick_',
    'mousedown': 'cancelMiddleMouseBehavior_',
    'mouseup': 'cancelMiddleMouseBehavior_',
    'touchstart': 'onTouchStart_',
  },

  /** @override */
  attached() {
    this.watch('item_', store => store.nodes[this.itemId]);
    this.watch(
        'isSelectedItem_', store => store.selection.items.has(this.itemId));
    this.watch('isMultiSelect_', store => store.selection.items.size > 1);

    this.updateFromStore();
  },

  focusMenuButton() {
    focusWithoutInk(this.$.menuButton);
  },

  /** @return {BookmarksItemElement} */
  getDropTarget() {
    return this;
  },

  /**
   * @param {Event} e
   * @private
   */
  onContextMenu_(e) {
    e.preventDefault();
    e.stopPropagation();

    // Prevent context menu from appearing after a drag, but allow opening the
    // context menu through 2 taps
    if (e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents &&
        this.lastTouchPoints_ !== 2) {
      return;
    }

    this.focus();
    if (!this.isSelectedItem_) {
      this.selectThisItem_();
    }

    this.fire('open-command-menu', {
      x: e.clientX,
      y: e.clientY,
      source: MenuSource.ITEM,
      targetId: this.itemId,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonClick_(e) {
    e.stopPropagation();
    e.preventDefault();

    // Skip selecting the item if this item is part of a multi-selected group.
    if (!this.isMultiSelectMenu_()) {
      this.selectThisItem_();
    }

    this.fire('open-command-menu', {
      targetElement: e.target,
      source: MenuSource.ITEM,
      targetId: this.itemId,
    });
  },

  /** @private */
  selectThisItem_() {
    this.dispatch(selectItem(this.itemId, this.getState(), {
      clear: true,
      range: false,
      toggle: false,
    }));
  },

  /** @private */
  onItemIdChanged_() {
    // TODO(tsergeant): Add a histogram to measure whether this assertion fails
    // for real users.
    assert(this.getState().nodes[this.itemId]);
    this.updateFromStore();
  },

  /** @private */
  onItemChanged_() {
    this.isFolder_ = !this.item_.url;
    this.setAttribute(
        'aria-label',
        this.item_.title || this.item_.url ||
            loadTimeData.getString('folderLabel'));
  },

  /**
   * @param {MouseEvent} e
   * @private
   */
  onClick_(e) {
    // Ignore double clicks so that Ctrl double-clicking an item won't deselect
    // the item before opening.
    if (e.detail !== 2) {
      const addKey = isMac ? e.metaKey : e.ctrlKey;
      this.dispatch(selectItem(this.itemId, this.getState(), {
        clear: !addKey,
        range: e.shiftKey,
        toggle: addKey && !e.shiftKey,
      }));
    }
    e.stopPropagation();
    e.preventDefault();
  },

  /**
   * @private
   * @param {KeyboardEvent} e
   */
  onKeydown_(e) {
    if (e.key === 'ArrowLeft') {
      this.focus();
    } else if (e.key === 'ArrowRight') {
      this.$.menuButton.focus();
    } else if (e.key === ' ') {
      this.dispatch(selectItem(this.itemId, this.getState(), {
        clear: false,
        range: false,
        toggle: true,
      }));
    }
  },

  /**
   * @param {MouseEvent} e
   * @private
   */
  onDblClick_(e) {
    if (!this.isSelectedItem_) {
      this.selectThisItem_();
    }

    const commandManager = CommandManager.getInstance();
    const itemSet = this.getState().selection.items;
    if (commandManager.canExecute(Command.OPEN, itemSet)) {
      commandManager.handle(Command.OPEN, itemSet);
    }
  },

  /**
   * @param {MouseEvent} e
   * @private
   */
  onMiddleClick_(e) {
    if (e.button !== 1) {
      return;
    }

    this.selectThisItem_();
    if (this.isFolder_) {
      return;
    }

    const commandManager = CommandManager.getInstance();
    const itemSet = this.getState().selection.items;
    const command = e.shiftKey ? Command.OPEN : Command.OPEN_NEW_TAB;
    if (commandManager.canExecute(command, itemSet)) {
      commandManager.handle(command, itemSet);
    }
  },

  /**
   * @param {TouchEvent} e
   * @private
   */
  onTouchStart_(e) {
    this.lastTouchPoints_ = e.touches.length;
  },

  /**
   * Prevent default middle-mouse behavior. On Windows, this prevents autoscroll
   * (during mousedown), and on Linux this prevents paste (during mouseup).
   * @param {MouseEvent} e
   * @private
   */
  cancelMiddleMouseBehavior_(e) {
    if (e.button === 1) {
      e.preventDefault();
    }
  },

  /**
   * @param {string} url
   * @private
   */
  updateFavicon_(url) {
    this.$.icon.className = url ? 'website-icon' : 'folder-icon';
    this.$.icon.style.backgroundImage =
        url ? getFaviconForPageURL(url, false) : '';
  },

  /**
   * @return {string}
   * @private
   */
  getButtonAriaLabel_() {
    if (!this.item_) {
      return '';  // Item hasn't loaded, skip for now.
    }

    if (this.isMultiSelectMenu_()) {
      return loadTimeData.getStringF('moreActionsMultiButtonAxLabel');
    }

    return loadTimeData.getStringF(
        'moreActionsButtonAxLabel', this.item_.title);
  },

  /**
   * This item is part of a group selection.
   * @return {boolean}
   * @private
   */
  isMultiSelectMenu_() {
    return this.isSelectedItem_ && this.isMultiSelect_;
  },
});
