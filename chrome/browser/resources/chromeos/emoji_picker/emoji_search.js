// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EmojiButton} from './emoji_button.js';
import {EmojiCategoryButton} from './emoji_category_button.js';
import Fuse from './fuse.js';
import {CategoryEnum, EmojiGroupData, EmojiVariants} from './types.js';

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
      /** @type {EmojiGroupData} */
      emoticonData: {type: Array, readonly: true},
      /** @type {!string} */
      search: {type: String, notify: true},
      /** @private {!Array<!EmojiVariants>} */
      emojiList: {
        type: Array,
        computed: 'computeEmojiList(emojiData,emojiData.length)',
        observer: 'onEmojiListChanged'
      },
      /** @private {!Array<!EmojiVariants>} */
      emoticonList: {
        type: Array,
        computed: 'computeEmojiList(emoticonData)',
        observer: 'onEmoticonListChanged'
      },
      /** @private {!Array<!EmojiVariants>} */
      emojiResults:
          {type: Array, computed: 'computeSearchResults(search, \'emoji\')'},
      /** @private {!Array<!EmojiVariants>} */
      emoticonResults:
          {type: Array, computed: 'computeSearchResults(search, \'emoticon\')'},
      /** @private {!boolean} */
      v2Enabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        readonly: true
      }
    };
  }

  constructor() {
    super();
    const fuseConfig = {
      threshold: 0.0,        // Exact match only.
      ignoreLocation: true,  // Match in all locations.
      keys: [
        {name: 'base.name', weight: 10},  // Increase scoring of emoji name.
        'base.keywords',
      ]
    };
    this.emojiFuse = new Fuse([], fuseConfig);
    this.emoticonFuse = new Fuse([], fuseConfig);
    this.addEventListener('scroll', () => {
      this.onSearchScroll();
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
    // TODO(b/233567886): Implement navigation by keyboard for V2.
    if (this.v2Enabled) {
      return;
    }

    const isUp = ev.key === 'ArrowUp';
    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    // get emoji-button which has focus.
    /** @type {Element} */
    const focusedResult = this.shadowRoot.querySelector('.result:focus-within');

    if (isEnter && focusedResult) {
      focusedResult.click();
    }
    if (!isUp && !isDown) {
      return;
    }

    ev.preventDefault();
    ev.stopPropagation();

    if (!focusedResult) {
      return;
    }

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
    if (!this.search ||
        (this.emojiResults.length === 0 && this.emoticonResults.length === 0)) {
      return;
    }

    const isDown = ev.key === 'ArrowDown';
    const isEnter = ev.key === 'Enter';
    const isTab = ev.key === 'Tab';
    if (isDown || isEnter || isTab) {
      ev.preventDefault();
      ev.stopPropagation();

      if (!this.v2Enabled) {
        // focus first item in result list.
        const firstButton = this.shadowRoot.querySelector('.result');
        firstButton.focus();

        // if there is only one result, select it on enter.
        if (isEnter && this.emojiResults.length === 1) {
          firstButton.querySelector('emoji-button').click();
        }
      } else {
        const resultsCount =
          this.emojiResults.length + this.emoticonResults.length;

        if (resultsCount === 0) {
          return;
        }

        const firstResultButton = this.findFirstResultButton();

        if (!firstResultButton) {
          throw new Error('Cannot find search result buttons.');
        }

        if (isEnter && resultsCount === 1) {
          firstResultButton.click();
        } else {
          firstResultButton.focus();
        }
      }
    }
  }

  /**
   * Format the emoji data for search:
   * 1) Remove duplicates.
   * 2) Remove groupings.
   * @param {!EmojiGroupData} emojiData
   * @param {number} emojiDataLength Used to trick polymer into calling this
   *     when the emojidata is updated via push
   * @return {!Array<!EmojiVariants>}
   */
  computeEmojiList(emojiData, emojiDataLength) {
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

  onSearchScroll() {
    if (!this.v2Enabled) {
      this.$['search-shadow'].style.boxShadow =
          this.shadowRoot.getElementById('results').scrollTop > 0 ?
          'var(--cr-elevation-3)' :
          'none';
    }
  }

  /**
   *
   * @param {!Array<!EmojiVariants>} emojiList
   * @suppress {missingProperties}
   */
  onEmojiListChanged(emojiList) {
    // suppressed property error due to Fuse being untyped.
    this.emojiFuse.setCollection(emojiList);
  }

  /**
   *
   * @param {!Array<!EmojiVariants>} emoticonList
   * @suppress {missingProperties}
   */
  onEmoticonListChanged(emoticonList) {
    this.emoticonFuse.setCollection(emoticonList);
  }

  /**
   * @param {?string} search
   * @param {CategoryEnum} category
   */
  computeSearchResults(search, category) {
    if (!search) {
      return [];
    }
    // Add an initial space to force prefix matching only.
    const prefixSearchTerm = ` ${search}`;
    let fuseResults = [];
    if (!this.v2Enabled) {
      fuseResults = this.emojiFuse.search(prefixSearchTerm);
    } else {
      switch (category) {
        case CategoryEnum.EMOJI:
          fuseResults = this.emojiFuse.search(prefixSearchTerm);
          break;
        case CategoryEnum.EMOTICON:
          fuseResults = this.emoticonFuse.search(prefixSearchTerm);
          break;
        default:
          throw new Error('Unknown category.');
      }
    }
    return fuseResults.map(item => item.item);
  }

  onResultClick(ev) {
    // If the click is on elements except emoji-button, trigger the click on
    // the emoji-button.
    if (ev.target.nodeName !== 'EMOJI-BUTTON') {
      ev.currentTarget.querySelector('emoji-button')
          .shadowRoot.querySelector('button')
          .click();
    }
  }

  /**
   * Find the first button in the search result page.
   *
   * @returns {?HTMLElement} First button or null for no results.
   */
  findFirstResultButton() {
    const results = this.shadowRoot.querySelector(
      '#search-results').querySelectorAll('emoji-group');
    for (const result of results) {
      const button = result.firstEmojiButton();
      if (button) {
        return button;
      }
    }
    return null;
  }

  /**
   * @param {!Array<!EmojiVariants>} emojiResults
   * @param {!Array<!EmojiVariants>} emoticonResults
   * @returns {boolean}
   */
  isSearchResultEmpty(emojiResults, emoticonResults) {
    if (!this.v2Enabled) {
      return emojiResults.length === 0;
    }
    return emojiResults.length === 0 && emoticonResults.length === 0;
  }
}

customElements.define(EmojiSearch.is, EmojiSearch);
