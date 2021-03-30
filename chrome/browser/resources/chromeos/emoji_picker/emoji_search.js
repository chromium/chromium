// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';

import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EmojiButton} from './emoji_button.js';
import Fuse from './fuse.js';
import {EmojiGroupData, EmojiVariants} from './types.js';

/**
 * @typedef {!Array<{item: !EmojiVariants}>} FuseResults
 */
let FuseResults;

export class EmojiSearch extends PolymerElement {
  static get is() {
    return 'emoji-search';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {EmojiGroupData} */
      emojiData: {type: Array, readonly: true},
      /** @type {!string} */
      search: {type: String, notify: true},
      /** @private {!Array<!EmojiVariants>} */
      emojiList: {
        type: Array,
        computed: 'computeEmojiList(emojiData)',
        observer: 'onEmojiListChanged'
      },
      /** @private {!FuseResults} */
      results:
          {type: Array, computed: 'computeSearchResults(search, emojiList)'},
    };
  }

  constructor() {
    super();
    this.fuse = new Fuse([], {
      threshold: 0.0,        // Exact match only.
      ignoreLocation: true,  // Match in all locations.
      keys: [
        {name: 'base.name', weight: 10},  // Increase scoring of emoji name.
        'base.keywords',
      ]
    });
  }

  ready() {
    super.ready();

    this.addEventListener('search', ev => this.onSearch(ev.detail));
    this.$.search.getSearchInput().addEventListener(
        'keydown',
        (ev) => this.onSearchKeyDown(/** @type {KeyboardEvent} */ (ev)));
    this.addEventListener(
        'keydown', ev => this.onKeyDown(/** @type {KeyboardEvent} */ (ev)));
  }

  onSearch(newSearch) {
    this.search = newSearch;
  }

  /**
   * Event handler for keydown anywhere in the search component.
   * Used to move the focused result up/down on arrow presses.
   * @param {KeyboardEvent} ev
   */
  onKeyDown(ev) {
    const isUp = ev.key === 'ArrowUp';
    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    if (isEnter) {
      this.shadowRoot.querySelector('.result:focus-within').click();
    }
    if (!isUp && !isDown)
      return;

    ev.preventDefault();
    ev.stopPropagation();
    // get emoji-button which has focus.
    /** @type {Element} */
    const focusedResult = this.shadowRoot.querySelector('.result:focus-within');
    if (!focusedResult)
      return;

    const prev = focusedResult.previousElementSibling;
    const next = focusedResult.nextElementSibling;

    // moving up from first result focuses search box.
    // need to check classList in case prev is sr-only.
    if (isUp && prev && !prev.classList.contains('result')) {
      this.$.search.getSearchInput().focus();
      return;
    }

    const newResult = isDown ? next : prev;
    if (newResult) {
      newResult.focus();
    }
  }

  /**
   * Event handler for keydown on the search input. Used to switch focus to the
   * results list on down arrow or enter key presses.
   * @param {KeyboardEvent} ev
   */
  onSearchKeyDown(ev) {
    // if not searching or no results, do nothing.
    if (!this.search || !this.results.length)
      return;

    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    if (isDown || isEnter) {
      ev.preventDefault();
      ev.stopPropagation();

      // focus first item in result list.
      const firstButton = this.shadowRoot.querySelector('.result');
      firstButton.focus();

      // if there is only one result, select it on enter.
      if (isEnter && this.results.length === 1) {
        firstButton.querySelector('emoji-button').click();
      }
    }
  }

  /**
   * Format the emoji data for search:
   * 1) Remove duplicates.
   * 2) Remove groupings.
   * @param {!EmojiGroupData} emojiData
   * @return {!Array<!EmojiVariants>}
   */
  computeEmojiList(emojiData) {
    return Array.from(
        new Map(emojiData.map(group => group.emoji).flat(1).map(emoji => {
          // The Fuse search library in ChromeOS doesn't support prefix
          // matching. A workaround is appending a space before all name and
          // keyword labels. This allows us to force a prefix matching by
          // prepending a space on users' searches. E.g. for the Emoji "smile
          // face", we store " smile face", if the user searches for "fa", the
          // search will be " fa" and will match " smile face", but not "
          // infant".
          emoji.base.name = ' ' + emoji.base.name;
          emoji.base.keywords =
              emoji.base.keywords.map(keyword => ' ' + keyword);
          return [emoji.base.string, emoji];
        })).values());
  }

  /**
   *
   * @param {!Array<!EmojiVariants>} emojiList
   * @suppress {missingProperties}
   */
  onEmojiListChanged(emojiList) {
    // suppressed property error due to Fuse being untyped.
    this.fuse.setCollection(emojiList);
  }

  /**
   * @param {?string} search
   * @param {!Array<!EmojiVariants>} emojiList
   */
  computeSearchResults(search, emojiList) {
    if (!search)
      return [];
    // Add an initial space to force prefix matching only.
    return this.fuse.search(' ' + search);
  }

  onResultClick(ev) {
    ev.currentTarget.querySelector('emoji-button')
        .shadowRoot.querySelector('button')
        .click();
  }
}

customElements.define(EmojiSearch.is, EmojiSearch);
