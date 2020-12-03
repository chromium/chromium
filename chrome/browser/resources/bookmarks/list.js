// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './item.js';
import './shared_style.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {deselectItems, selectAll, selectItem, updateAnchor} from './actions.js';
import {CommandManager} from './command_manager.js';
import {MenuSource} from './constants.js';
import {StoreClient} from './store_client.js';
import {BookmarksPageState} from './types.js';
import {canReorderChildren, getDisplayedList} from './util.js';

Polymer({
  is: 'bookmarks-list',

  _template: html`{__html_template__}`,

  behaviors: [
    StoreClient,
    ListPropertyUpdateBehavior,
  ],

  properties: {
    /**
     * A list of item ids wrapped in an Object. This is necessary because
     * iron-list is unable to distinguish focusing index 6 from focusing id '6'
     * so the item we supply to iron-list needs to be non-index-like.
     * @private {Array<{id: string}>}
     */
    displayedList_: {
      type: Array,
      value() {
        // Use an empty list during initialization so that the databinding to
        // hide #list takes effect.
        return [];
      },
    },

    /** @private {Array<string>} */
    displayedIds_: {
      type: Array,
      observer: 'onDisplayedIdsChanged_',
    },

    /** @private */
    searchTerm_: {
      type: String,
      observer: 'onDisplayedListSourceChange_',
    },

    /** @private */
    selectedFolder_: {
      type: String,
      observer: 'onDisplayedListSourceChange_',
    },

    /** @private {Set<string>} */
    selectedItems_: Object,
  },

  listeners: {
    'click': 'deselectItems_',
    'contextmenu': 'onContextMenu_',
    'open-command-menu': 'onOpenCommandMenu_',
  },

  attached() {
    const list = /** @type {IronListElement} */ (this.$.list);
    list.scrollTarget = this;

    this.watch('displayedIds_', function(state) {
      return getDisplayedList(/** @type {!BookmarksPageState} */ (state));
    });
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.watch('selectedFolder_', function(state) {
      return state.selectedFolder;
    });
    this.watch('selectedItems_', ({selection: {items}}) => items);
    this.updateFromStore();

    this.$.list.addEventListener(
        'keydown', this.onItemKeydown_.bind(this), true);

    /** @private {function(!Event)} */
    this.boundOnHighlightItems_ = this.onHighlightItems_.bind(this);
    document.addEventListener('highlight-items', this.boundOnHighlightItems_);

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  },

  detached() {
    document.removeEventListener(
        'highlight-items', this.boundOnHighlightItems_);
  },

  /** @return {HTMLElement} */
  getDropTarget() {
    return /** @type {!HTMLDivElement} */ (this.$.message);
  },

  /**
   * Updates `displayedList_` using splices to be equivalent to `newValue`. This
   * allows the iron-list to delete sublists of items which preserves scroll and
   * focus on incremental update.
   * @param {Array<string>} newValue
   * @param {Array<string>} oldValue
   */
  onDisplayedIdsChanged_: async function(newValue, oldValue) {
    const updatedList = newValue.map(id => ({id: id}));
    let skipFocus = false;
    let selectIndex = -1;
    if (this.matches(':focus-within')) {
      if (this.selectedItems_.size > 0) {
        const selectedId = Array.from(this.selectedItems_)[0];
        skipFocus = newValue.some(id => id === selectedId);
        selectIndex =
            this.displayedList_.findIndex(({id}) => selectedId === id);
      }
      if (selectIndex === -1 && updatedList.length > 0) {
        selectIndex = 0;
      } else {
        selectIndex = Math.min(selectIndex, updatedList.length - 1);
      }
    }
    this.updateList('displayedList_', item => item.id, updatedList);
    // Trigger a layout of the iron list. Otherwise some elements may render
    // as blank entries. See https://crbug.com/848683
    this.$.list.fire('iron-resize');
    const label = await PluralStringProxyImpl.getInstance().getPluralString(
        'listChanged', this.displayedList_.length);
    this.fire('iron-announce', {text: label});

    if (!skipFocus && selectIndex > -1) {
      setTimeout(() => {
        this.$.list.focusItem(selectIndex);
        // Focus menu button so 'Undo' is only one tab stop away on delete.
        const item = getDeepActiveElement();
        if (item) {
          item.focusMenuButton();
        }
      });
    }
  },

  /** @private */
  onDisplayedListSourceChange_() {
    this.scrollTop = 0;
  },

  /**
   * Scroll the list so that |itemId| is visible, if it is not already.
   * @param {string} itemId
   * @private
   */
  scrollToId_(itemId) {
    const index = this.displayedIds_.indexOf(itemId);
    const list = this.$.list;
    if (index >= 0 && index < list.firstVisibleIndex ||
        index > list.lastVisibleIndex) {
      list.scrollToIndex(index);
    }
  },

  /** @private */
  emptyListMessage_() {
    let emptyListMessage = 'noSearchResults';
    if (!this.searchTerm_) {
      emptyListMessage =
          canReorderChildren(this.getState(), this.getState().selectedFolder) ?
          'emptyList' :
          'emptyUnmodifiableList';
    }
    return loadTimeData.getString(emptyListMessage);
  },

  /** @private */
  isEmptyList_() {
    return this.displayedList_.length === 0;
  },

  /** @private */
  deselectItems_() {
    this.dispatch(deselectItems());
  },

  /**
   * @param{HTMLElement} el
   * @private
   */
  getIndexForItemElement_(el) {
    return this.$.list.modelForElement(el).index;
  },

  /**
   * @param {!CustomEvent<{source: !MenuSource}>} e
   * @private
   */
  onOpenCommandMenu_(e) {
    // If the item is not visible, scroll to it before rendering the menu.
    if (e.detail.source === MenuSource.ITEM) {
      this.scrollToId_(
          /** @type {BookmarksItemElement} */ (e.composedPath()[0]).itemId);
    }
  },

  /**
   * Highlight a list of items by selecting them, scrolling them into view and
   * focusing the first item.
   * @param {Event} e
   * @private
   */
  onHighlightItems_(e) {
    // Ensure that we only select items which are actually being displayed.
    // This should only matter if an unrelated update to the bookmark model
    // happens with the perfect timing to end up in a tracked batch update.
    const toHighlight = /** @type {!Array<string>} */
        (e.detail.filter((item) => this.displayedIds_.indexOf(item) !== -1));

    if (toHighlight.length <= 0) {
      return;
    }

    const leadId = toHighlight[0];
    this.dispatch(selectAll(toHighlight, this.getState(), leadId));

    // Allow iron-list time to render additions to the list.
    this.async(function() {
      this.scrollToId_(leadId);
      const leadIndex = this.displayedIds_.indexOf(leadId);
      assert(leadIndex !== -1);
      this.$.list.focusItem(leadIndex);
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onItemKeydown_(e) {
    let handled = true;
    const list = this.$.list;
    let focusMoved = false;
    let focusedIndex =
        this.getIndexForItemElement_(/** @type {HTMLElement} */ (e.target));
    const oldFocusedIndex = focusedIndex;
    const cursorModifier = isMac ? e.metaKey : e.ctrlKey;
    if (e.key === 'ArrowUp') {
      focusedIndex--;
      focusMoved = true;
    } else if (e.key === 'ArrowDown') {
      focusedIndex++;
      focusMoved = true;
      e.preventDefault();
    } else if (e.key === 'Home') {
      focusedIndex = 0;
      focusMoved = true;
    } else if (e.key === 'End') {
      focusedIndex = list.items.length - 1;
      focusMoved = true;
    } else if (e.key === ' ' && cursorModifier) {
      this.dispatch(
          selectItem(this.displayedIds_[focusedIndex], this.getState(), {
            clear: false,
            range: false,
            toggle: true,
          }));
    } else {
      handled = false;
    }

    if (focusMoved) {
      focusedIndex = Math.min(list.items.length - 1, Math.max(0, focusedIndex));
      list.focusItem(focusedIndex);

      if (cursorModifier && !e.shiftKey) {
        this.dispatch(updateAnchor(this.displayedIds_[focusedIndex]));
      } else {
        // If shift-selecting with no anchor, use the old focus index.
        if (e.shiftKey && this.getState().selection.anchor === null) {
          this.dispatch(updateAnchor(this.displayedIds_[oldFocusedIndex]));
        }

        // If the focus moved from something other than a Ctrl + move event,
        // update the selection.
        const config = {
          clear: !cursorModifier,
          range: e.shiftKey,
          toggle: false,
        };

        this.dispatch(selectItem(
            this.displayedIds_[focusedIndex], this.getState(), config));
      }
    }

    // Prevent the iron-list from changing focus on enter.
    if (e.key === 'Enter') {
      if (e.composedPath()[0].tagName === 'CR-ICON-BUTTON') {
        return;
      }
      if (e.composedPath()[0] instanceof HTMLButtonElement) {
        handled = true;
      }
    }

    if (!handled) {
      handled = CommandManager.getInstance().handleKeyEvent(
          e, this.getState().selection.items);
    }

    if (handled) {
      e.stopPropagation();
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  onContextMenu_(e) {
    e.preventDefault();
    this.deselectItems_();

    this.fire('open-command-menu', {
      x: e.clientX,
      y: e.clientY,
      source: MenuSource.LIST,
    });
  },

  /**
   * Returns a 1-based index for aria-rowindex.
   * @param {number} index
   * @return {number}
   * @private
   */
  getAriaRowindex_(index) {
    return index + 1;
  },

  /**
   * @param {string} id
   * @return {boolean}
   */
  getAriaSelected_(id) {
    return this.selectedItems_.has(id);
  },
});
