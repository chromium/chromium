// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-items-dialog' is a dialog for adding an
 * unordered set of items at a time. The component supports suggested items, as
 * well as items being disabled by policy.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './cr_checkbox_with_policy.js';
import './shared_style.css.js';
import '../../settings_shared.css.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {FindShortcutBehavior, FindShortcutBehaviorInterface} from '../find_shortcut_behavior.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './add_items_dialog.html.js';

/**
 * `id` must unique.
 * `name` is the displayed name to the user.
 * `searchTerms` are additional strings which will be matched when doing a text
 * search.
 * `disabledByPolicy` can be set to show that a given item is disabled by
 * policy. These items will never appear as a suggestion.
 * @typedef {{id: string, name: string, searchTerms: !Array<string>,
 * disabledByPolicy: boolean}}
 */
export let Item;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {FindShortcutBehaviorInterface}
 */
const OsSettingsAddItemsDialogElementBase = mixinBehaviors(
    [CrScrollableBehavior, FindShortcutBehavior], PolymerElement);

/** @polymer */
class OsSettingsAddItemsDialogElement extends
    OsSettingsAddItemsDialogElementBase {
  static get is() {
    return 'os-settings-add-items-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!Array<!Item>} */
      items: {
        type: Array,
        value: [],
      },

      /**
       * Item IDs to show in the "Suggested" section of the dialog.
       * Any items in this array which are disabled by policy, or IDs which do
       * not appear in the items array will be filtered out automatically.
       * @type {!Array<!string>}
       */
      suggestedItemIds: {
        type: Array,
        value: [],
      },

      header: String,

      searchLabel: String,

      suggestedItemsLabel: String,

      allItemsLabel: String,

      policyTooltip: String,

      /** @private */
      lowercaseQueryString_: String,

      /** @private {!Array<!Item>} */
      filteredItems_: {
        type: Array,
        computed: 'getFilteredItems_(items.*, lowercaseQueryString_)',
        value: [],
      },

      /** @private {!Set<string>} */
      itemIdsToAdd_: {
        type: Object,
        value() {
          return new Set();
        },
      },

      /**
       * Mapping from item ID to item for use in computing `suggestedItems_`.
       * @private {!Map<string, !Item>}
       */
      itemIdsToItems_: {
        type: Object,
        computed: 'getItemIdsToItems_(items.*)',
        value() {
          return new Map();
        },
      },

      /**
       * All items are guaranteed to not be disabled by policy.
       * @private {!Array<!Item>}
       */
      suggestedItems_: {
        type: Array,
        computed: 'getSuggestedItems_(suggestedItemIds.*, itemIdsToItems_)',
        value: [],
      },

      /** @private */
      showSuggestedList_: {
        type: Boolean,
        computed: `shouldShowSuggestedList_(suggestedItems_.length,
            lowercaseQueryString_)`,
        value: false,
      },

      /** @private */
      showFilteredList_: {
        type: Boolean,
        computed: 'shouldShowFilteredList_(filteredItems_.length)',
        value: true,
      },

      disableActionButton_: {
        type: Boolean,
        computed: 'shouldDisableActionButton_(itemIdsToAdd_.size)',
        value: true,
      },
    };
  }

  static get observers() {
    return [
      // The two observers below have all possible properties that could affect
      // the scroll offset of the two lists as dependencies.
      `updateSuggestedListScrollOffset_(showSuggestedList_,
          suggestedItemsLabel)`,
      `updateFilteredListScrollOffset_(showSuggestedList_,
          suggestedItemsLabel, suggestedItems_.length, showFilteredList_)`,
    ];
  }

  // Override FindShortcutBehavior methods.
  handleFindShortcut(_modalContextOpen) {
    // Assumes this is the only open modal.
    const searchInput = this.$.search.getSearchInput();
    searchInput.scrollIntoView();
    if (!this.searchInputHasFocus()) {
      searchInput.focus();
    }
    return true;
  }

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    return this.$.search.getSearchInput() ===
        this.$.search.shadowRoot.activeElement;
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQueryString_ = e.detail.toLocaleLowerCase();
  }

  /**
   * @param {{model: {item: !Item}, target: !Element}} e
   * @private
   */
  onCheckboxChange_(e) {
    const id = e.model.item.id;
    if (e.target.checked) {
      this.itemIdsToAdd_.add(id);
    } else {
      this.itemIdsToAdd_.delete(id);
    }
    // Polymer doesn't notify changes to set size.
    this.notifyPath('itemIdsToAdd_.size');
  }

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  }

  /**
   * @private
   */
  onActionButtonClick_() {
    this.dispatchEvent(new CustomEvent('items-added', {
      bubbles: true,
      composed: true,
      detail: this.itemIdsToAdd_,
    }));
    this.$.dialog.close();
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoView();
    }
  }

  /**
   * True if the user has chosen to add this item (checked its checkbox).
   * @param {string} id
   * @return {boolean}
   * @private
   */
  willAdd_(id) {
    return this.itemIdsToAdd_.has(id);
  }

  /**
   * @return {!Map<string, !Item>}
   * @private
   */
  getItemIdsToItems_() {
    return new Map(this.items.map(item => [item.id, item]));
  }

  /**
   * Returns whether a string matches the current search query.
   * @param {string} string
   * @return {boolean}
   * @private
   */
  matchesSearchQuery_(string) {
    return string.toLocaleLowerCase().includes(this.lowercaseQueryString_);
  }

  /**
   * @return {!Array<!Item>}
   * @private
   */
  getFilteredItems_() {
    if (!this.lowercaseQueryString_) {
      return this.items;
    }

    return this.items.filter(
        item => this.matchesSearchQuery_(item.name) ||
            item.searchTerms.some(term => this.matchesSearchQuery_(term)));
  }

  /**
   * @return {!Array<!Item>}
   * @private
   */
  getSuggestedItems_() {
    return this.suggestedItemIds.map(id => this.itemIdsToItems_.get(id))
        .filter(item => item !== undefined && !item.disabledByPolicy);
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSuggestedList_() {
    return this.suggestedItems_.length > 0 && !this.lowercaseQueryString_;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowFilteredList_() {
    return this.filteredItems_.length > 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableActionButton_() {
    return !this.itemIdsToAdd_.size;
  }

  /**
   * @private
   */
  updateSuggestedListScrollOffset_() {
    afterNextRender(this, () => {
      if (!this.showSuggestedList_) {
        return;
      }
      // Because #suggestedItemsList is not statically created (as it is
      // within a <template is="dom-if">), we can't use this.$ here.
      const list = /** @type {!IronListElement|null} */ (
          this.shadowRoot.getElementById('suggested-items-list'));
      if (list === null) {
        return;
      }
      list.scrollOffset = list.offsetTop;
    });
  }

  /**
   * @private
   */
  updateFilteredListScrollOffset_() {
    afterNextRender(this, () => {
      if (!this.showFilteredList_) {
        return;
      }
      const list = /** @type {!IronListElement|null} */ (
          this.shadowRoot.getElementById('filtered-items-list'));
      if (list === null) {
        return;
      }
      list.scrollOffset = list.offsetTop;
    });
  }
}

customElements.define(
    OsSettingsAddItemsDialogElement.is, OsSettingsAddItemsDialogElement);
