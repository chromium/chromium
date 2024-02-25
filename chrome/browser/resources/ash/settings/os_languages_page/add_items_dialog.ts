// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-items-dialog' is a dialog for adding an
 * unordered set of items at a time. The component supports suggested items, as
 * well as items being disabled by policy.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './cr_checkbox_with_policy.js';
import './shared_style.css.js';
import '../settings_shared.css.js';

import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {CrSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixin} from 'chrome://resources/ash/common/cr_elements/find_shortcut_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './add_items_dialog.html.js';

const ITEMS_ADDED_EVENT_NAME = 'items-added' as const;

/**
 * `id` must unique.
 * `name` is the displayed name to the user.
 * `searchTerms` are additional strings which will be matched when doing a text
 * search.
 * `disabledByPolicy` can be set to show that a given item is disabled by
 * policy. These items will never appear as a suggestion.
 */
export interface Item {
  id: string;
  name: string;
  searchTerms: string[];
  disabledByPolicy: boolean;
}

export interface OsSettingsAddItemsDialogElement {
  $: {
    dialog: CrDialogElement,
    search: CrSearchFieldElement,
  };
}
const OsSettingsAddItemsDialogElementBase =
    CrScrollableMixin(FindShortcutMixin(PolymerElement));

export class OsSettingsAddItemsDialogElement extends
    OsSettingsAddItemsDialogElementBase {
  static get is() {
    return 'os-settings-add-items-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      items: {
        type: Array,
        // This array is shared between all instances of the class:
        // https://crrev.com/c/3897703/comment/fa845200_e10503c6/
        // TODO(b/265556004): Move this to the constructor to avoid this.
        value: [],
      },

      /**
       * Item IDs to show in the "Suggested" section of the dialog.
       * Any items in this array which are disabled by policy, or IDs which do
       * not appear in the items array will be filtered out automatically.
       */
      suggestedItemIds: {
        type: Array,
        // This array is shared between all instances of the class:
        // https://crrev.com/c/3897703/comment/fa845200_e10503c6/
        // TODO(b/265556004): Move this to the constructor to avoid this.
        value: [],
      },

      header: String,

      searchLabel: String,

      suggestedItemsLabel: String,

      allItemsLabel: String,

      policyTooltip: String,

      lowercaseQueryString_: String,

      filteredItems_: {
        type: Array,
        computed: 'getFilteredItems_(items.*, lowercaseQueryString_)',
        // This array is shared between all instances of the class:
        // https://crrev.com/c/3897703/comment/fa845200_e10503c6/
        // TODO(b/265556004): Move this to the constructor to avoid this.
        value: [],
      },

      itemIdsToAdd_: {
        type: Object,
        value() {
          return new Set();
        },
      },

      /**
       * Mapping from item ID to item for use in computing `suggestedItems_`.
       */
      itemIdsToItems_: {
        type: Object,
        computed: 'getItemIdsToItems_(items.*)',
        value() {
          return new Map();
        },
      },

      /**
       * All items in this array are guaranteed to not be disabled by policy.
       */
      suggestedItems_: {
        type: Array,
        computed: 'getSuggestedItems_(suggestedItemIds.*, itemIdsToItems_)',
        // This array is shared between all instances of the class:
        // https://crrev.com/c/3897703/comment/fa845200_e10503c6/
        // TODO(b/265556004): Move this to the constructor to avoid this.
        value: [],
      },

      showSuggestedList_: {
        type: Boolean,
        computed: `shouldShowSuggestedList_(suggestedItems_.length,
            lowercaseQueryString_)`,
        value: false,
      },

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

  // Public API: Items to show in the dialog (downwards data flow).
  items: Item[];
  /**
   * Item IDs to show in the "Suggested" section of the dialog.
   * Any items in this array which are disabled by policy, or IDs which do not
   * appear in the items array will be filtered out automatically.
   */
  suggestedItemIds: string[];

  // Public API: Strings displayed to the user, in the order a user would see
  // them (downwards data flow).
  header: string;
  searchLabel: string;
  suggestedItemsLabel: string;
  allItemsLabel: string;
  policyTooltip: string;

  // Internal state.
  private itemIdsToAdd_: Set<string>;
  // This property does not have a default value in `static get properties()`.
  // TODO(b/265556480): Update the initial value to be ''.
  private lowercaseQueryString_: string;

  // Computed properties for suggested items.
  /** Mapping from item ID to item for use in computing `suggestedItems_`. */
  private itemIdsToItems_: Map<string, Item>;
  /** All items in this array are guaranteed to not be disabled by policy. */
  private suggestedItems_: Item[];
  /** Whether suggestedItems_ is non-empty. */
  private showSuggestedList_: boolean;

  // Computed properties for filtered items.
  private filteredItems_: Item[];
  /** Whether filteredItems_ is non-empty. */
  private showFilteredList_: boolean;

  // Other computed properties.
  private disableActionButton_: boolean;

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

  override handleFindShortcut(_modalContextOpen: boolean): boolean {
    // Assumes this is the only open modal.
    const searchInput = this.$.search.getSearchInput();
    searchInput.scrollIntoView();
    if (!this.searchInputHasFocus()) {
      searchInput.focus();
    }
    return true;
  }

  override searchInputHasFocus(): boolean {
    return this.$.search.getSearchInput() ===
        this.$.search.shadowRoot!.activeElement;
  }

  // 'search-changed' event listener on a <cr-search-field>.
  private onSearchChanged_(e: CustomEvent<string>): void {
    this.lowercaseQueryString_ = e.detail.toLocaleLowerCase();
  }

  // 'change' event listener on a <cr-checkbox>.
  private onCheckboxChange_(e: DomRepeatEvent<Item, CustomEvent<boolean>>):
      void {
    const id = e.model.item.id;
    // Safety: This method is only called from a 'change' event from a
    // <cr-checkbox>, so the event target must be a <cr-checkbox>.
    if ((e.target! as CrCheckboxElement).checked) {
      this.itemIdsToAdd_.add(id);
    } else {
      this.itemIdsToAdd_.delete(id);
    }
    // Polymer doesn't notify changes to set size.
    this.notifyPath('itemIdsToAdd_.size');
  }

  private onCancelButtonClick_(): void {
    this.$.dialog.close();
  }

  private onActionButtonClick_(): void {
    const event: HTMLElementEventMap[typeof ITEMS_ADDED_EVENT_NAME] =
        new CustomEvent(ITEMS_ADDED_EVENT_NAME, {
          bubbles: true,
          composed: true,
          detail: this.itemIdsToAdd_,
        });
    this.dispatchEvent(event);
    this.$.dialog.close();
  }

  private onKeydown_(e: KeyboardEvent): void {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoView();
    }
  }

  /**
   * True if the user has chosen to add this item (checked its checkbox).
   */
  private willAdd_(id: string): boolean {
    return this.itemIdsToAdd_.has(id);
  }

  private getItemIdsToItems_(): Map<string, Item> {
    return new Map(this.items.map(item => [item.id, item]));
  }

  /**
   * Returns whether a string matches the current search query.
   */
  private matchesSearchQuery_(string: string): boolean {
    return string.toLocaleLowerCase().includes(this.lowercaseQueryString_);
  }

  private getFilteredItems_(): Item[] {
    if (!this.lowercaseQueryString_) {
      return this.items;
    }

    return this.items.filter(
        item => this.matchesSearchQuery_(item.name) ||
            item.searchTerms.some(term => this.matchesSearchQuery_(term)));
  }

  private getSuggestedItems_(): Item[] {
    return this.suggestedItemIds.map(id => this.itemIdsToItems_.get(id))
        .filter(
            <T>(item: T): item is Exclude<T, undefined> => item !== undefined)
        .filter(item => !item.disabledByPolicy);
  }

  private shouldShowSuggestedList_(): boolean {
    return this.suggestedItems_.length > 0 && !this.lowercaseQueryString_;
  }

  private shouldShowFilteredList_(): boolean {
    return this.filteredItems_.length > 0;
  }

  private shouldDisableActionButton_(): boolean {
    return !this.itemIdsToAdd_.size;
  }

  private updateSuggestedListScrollOffset_(): void {
    afterNextRender(this, () => {
      if (!this.showSuggestedList_) {
        return;
      }
      // Because #suggested-items-list is not statically created (as it is
      // within a <template is="dom-if">), we can't use this.$ here.
      const list = this.shadowRoot!.querySelector<IronListElement>(
          '#suggested-items-list');
      if (list === null) {
        return;
      }
      list.scrollOffset = list.offsetTop;
    });
  }

  private updateFilteredListScrollOffset_(): void {
    afterNextRender(this, () => {
      if (!this.showFilteredList_) {
        return;
      }
      // Because #filtered-items-list is not statically created (as it is
      // within a <template is="dom-if">), we can't use this.$ here.
      const list = this.shadowRoot!.querySelector<IronListElement>(
          '#filtered-items-list');
      if (list === null) {
        return;
      }
      list.scrollOffset = list.offsetTop;
    });
  }
}

customElements.define(
    OsSettingsAddItemsDialogElement.is, OsSettingsAddItemsDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsAddItemsDialogElement.is]: OsSettingsAddItemsDialogElement;
  }
  interface HTMLElementEventMap {
    [ITEMS_ADDED_EVENT_NAME]: CustomEvent<Set<string>>;
  }
}
