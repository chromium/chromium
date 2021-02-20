// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './emoji_group.js';
import './emoji_group_button.js';
import './emoji_search.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EMOJI_PER_ROW, EMOJI_PICKER_HEIGHT_PX, EMOJI_PICKER_WIDTH_PX, EMOJI_SIZE, EMOJI_SIZE_PX} from './constants.js';
import {EmojiButton} from './emoji_button.js';
import {createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_DATA_LOADED, EMOJI_VARIANTS_SHOWN, EmojiVariantsShownEvent, GROUP_BUTTON_CLICK} from './events.js';
import {RecentEmojiStore} from './store.js';
import {Emoji, EmojiGroup, EmojiGroupData, EmojiVariants} from './types.js';

const EMOJI_ORDERING_JSON = '/emoji_13_1_ordering.json';

// the name attributes below are used to label the group buttons.
// the ordering group names are used for the group headings in the emoji picker.
const GROUP_TABS = [
  {
    name: 'Recently Used',
    icon: 'emoji_picker:schedule',
    groupId: 'history',
    active: true
  },
  {
    name: 'Smileys & Emotion',
    icon: 'emoji_picker:insert_emoticon',
    groupId: '0',
    active: false
  },
  {
    name: 'People',
    icon: 'emoji_picker:emoji_people',
    groupId: '1',
    active: false
  },
  {
    name: 'Animals & Nature',
    icon: 'emoji_picker:emoji_nature',
    groupId: '2',
    active: false
  },
  {
    name: 'Food & Drink',
    icon: 'emoji_picker:emoji_food_beverage',
    groupId: '3',
    active: false
  },
  {
    name: 'Travel & Places',
    icon: 'emoji_picker:emoji_transportation',
    groupId: '4',
    active: false
  },
  {
    name: 'Activities',
    icon: 'emoji_picker:emoji_events',
    groupId: '5',
    active: false
  },
  {
    name: 'Objects',
    icon: 'emoji_picker:emoji_objects',
    groupId: '6',
    active: false
  },
  {
    name: 'Symbols',
    icon: 'emoji_picker:emoji_symbols',
    groupId: '7',
    active: false
  },
  {name: 'Flags', icon: 'emoji_picker:flag', groupId: '8', active: false},
];

/**
 * Constructs the emoji group data structure from a given list of emoji
 * strings. Note: returned emoji have no variants.
 *
 * @param {!Array<string>} recentEmoji list of recently used emoji strings.
 * @return {!Array<EmojiVariants>} list of emoji data structures
 */
function makeRecentlyUsed(recentEmoji) {
  return recentEmoji.map(
      emoji =>
          ({base: {string: emoji, name: '', keywords: []}, alternates: []}));
}

export class EmojiPicker extends PolymerElement {
  static get is() {
    return 'emoji-picker';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!string} */
      emojiDataUrl: {type: String, value: EMOJI_ORDERING_JSON},
      emojiGroupTabs: {type: Array},
      /** @private {?EmojiGroupData} */
      emojiData: {
        type: Object,
        observer: 'onEmojiDataChanged',
      },
      /** @private {!EmojiGroup} */
      history: {type: Object},
      /** @private {!string} */
      search: {type: String, value: '', observer: 'onSearchChanged'},
    };
  }

  constructor() {
    super();

    /** @type {!RecentEmojiStore} */
    this.recentEmojiStore = new RecentEmojiStore();

    this.emojiGroupTabs = GROUP_TABS;
    this.emojiData = [];
    this.history = {
      'group': 'Recently Used',
      'emoji': makeRecentlyUsed(this.recentEmojiStore.data),
    };

    /** @private {?number} */
    this.scrollTimeout = null;

    /** @private {?EmojiButton} */
    this.activeVariant = null;
  }

  ready() {
    super.ready();

    const xhr = new XMLHttpRequest();
    xhr.onloadend = () => this.onEmojiDataLoaded(xhr.responseText);
    xhr.open('GET', this.emojiDataUrl);
    xhr.send();

    this.updateStyles({
      '--emoji-picker-width': EMOJI_PICKER_WIDTH_PX,
      '--emoji-picker-height': EMOJI_PICKER_HEIGHT_PX,
      '--emoji-size': EMOJI_SIZE_PX,
      '--emoji-per-row': EMOJI_PER_ROW,
    });

    afterNextRender(this, () => {
      // basic click handlers
      this.addEventListener(
          GROUP_BUTTON_CLICK, ev => this.selectGroup(ev.detail.group));
      this.addEventListener(
          EMOJI_BUTTON_CLICK,
          ev => this.insertEmoji(ev.detail.emoji, ev.detail.isVariant));

      // variant popup related handlers
      this.addEventListener(
          EMOJI_VARIANTS_SHOWN,
          ev => this.onShowEmojiVariants(
              /** @type {!EmojiVariantsShownEvent} */ (ev)));
      this.addEventListener('click', () => this.hideEmojiVariants());
    });
  }

  onSearchChanged(newValue) {
    this.$.listContainer.style.display = newValue ? 'none' : '';
  }

  /**
   * @param {!string} emoji
   */
  insertEmoji(emoji, isVariant) {
    chrome.send('insertEmoji', [emoji, isVariant]);
    this.recentEmojiStore.bumpEmoji(emoji);
    this.set(
        ['history', 'emoji'], makeRecentlyUsed(this.recentEmojiStore.data));

    this.$.message.textContent = emoji + ' inserted.';
  }

  /**
   * @param {string} newGroup
   */
  selectGroup(newGroup) {
    // focus and scroll to selected group's first emoji.
    const group =
        this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`);
    group.querySelector('emoji-group')
        .shadowRoot.querySelector('emoji-button')
        .focusButton({preventScroll: true});
    group.scrollIntoView();
  }

  onEmojiScroll(ev) {
    // the scroll event is fired very frequently while scrolling.
    // only update active tab 100ms after last scroll event by setting
    // a timeout.
    if (this.scrollTimeout) {
      clearTimeout(this.scrollTimeout);
    }
    this.scrollTimeout = setTimeout(this.updateActiveGroup.bind(this), 100);
  }

  updateActiveGroup() {
    // no need to update scroll state if search is showing.
    if (this.search)
      return;

    // get bounding rect of scrollable emoji region.
    const thisRect = this.$.groups.getBoundingClientRect();

    const groupElements =
        Array.from(this.$.groups.querySelectorAll('[data-group]'));

    // activate the first group which is visible for at least 10 pixels,
    // i.e. whose bottom edge is at least 10px below the top edge of the
    // scrollable region.
    const activeGroup = groupElements.find(
        el => el.getBoundingClientRect().bottom - thisRect.top >= 10);

    assert(activeGroup, 'no group element was activated');
    const activeGroupId = activeGroup.dataset.group;

    // set active to true for selected group and false for others.
    this.emojiGroupTabs.forEach((g, i) => {
      const isActive = g.groupId === activeGroupId;
      this.set(['emojiGroupTabs', i, 'active'], isActive);
    });

    // scroll the active group button into view on the tab bar.
    this.shadowRoot
        .querySelector(`emoji-group-button[data-group="${activeGroupId}"]`)
        .scrollIntoView();
  }


  hideEmojiVariants() {
    if (this.activeVariant) {
      this.activeVariant.variantsVisible = false;
      this.activeVariant = null;
    }
  }

  /**
   * @param {!EmojiVariantsShownEvent} ev
   */
  onShowEmojiVariants(ev) {
    this.hideEmojiVariants();
    this.activeVariant = /** @type {EmojiButton} */ (ev.detail.button);
    if (this.activeVariant) {
      this.$.message.textContent = this.activeVariant + ' variants shown.';
      this.positionEmojiVariants(ev.detail.variants);
    }
  }

  positionEmojiVariants(variants) {
    // TODO(crbug.com/1174311): currently positions horizontally within page.
    // ideal UI would be overflowing the bounds of the page.
    // also need to account for vertical positioning.

    // compute width required for the variant popup as: SIZE * columns + 10.
    // SIZE is emoji width in pixels. number of columns is determined by width
    // of variantRows, then one column each for the base emoji and skin tone
    // indicators if present. 10 pixels are added for padding and the shadow.

    // get size of emoji picker
    const pickerRect = this.getBoundingClientRect();

    // determine how much overflows the right edge of the window.
    const rect = variants.getBoundingClientRect();
    const overflowWidth = rect.x + rect.width - pickerRect.width;
    // shift left by overflowWidth rounded up to next multiple of EMOJI_SIZE.
    const shift = EMOJI_SIZE * Math.ceil(overflowWidth / EMOJI_SIZE);
    // negative value means we are already within bounds, so no shift needed.
    variants.style.marginLeft = `-${Math.max(shift, 0)}px`;
  }

  onEmojiDataLoaded(data) {
    this.emojiData = /** @type {!EmojiGroupData} */ (JSON.parse(data));
  }

  /**
   * Fires DATA_LOADED_EVENT when emoji data is loaded and the emoji picker
   * is ready to use.
   */
  onEmojiDataChanged(newValue, oldValue) {
    // This is separate from onEmojiDataLoaded because we need to ensure
    // Polymer has created the components for the emoji after setting
    // this.emojiData. This is an observer, so will run after the component
    // tree has been updated.

    // see:
    // https://polymer-library.polymer-project.org/3.0/docs/devguide/data-system#property-effects

    if (newValue && newValue.length) {
      this.dispatchEvent(createCustomEvent(EMOJI_DATA_LOADED));
    }
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
