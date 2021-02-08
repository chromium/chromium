// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './emoji_group.js';
import './emoji_group_button.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EMOJI_PER_ROW, EMOJI_PICKER_HEIGHT_PX, EMOJI_PICKER_WIDTH_PX, EMOJI_SIZE, EMOJI_SIZE_PX} from './constants.js';
import {EmojiButton} from './emoji_button.js';
import {createCustomEvent, DATA_LOADED_EVENT, EMOJI_BUTTON_EVENT, GROUP_BUTTON_EVENT, SHOW_VARIANTS_EVENT, ShowVariantsEvent} from './events.js';
import {RecentEmojiStore} from './store.js';
import {Codepoints, Emoji, EmojiData, EmojiGroup} from './types.js';

const EMOJI_ORDERING_JSON = '/emoji_13_1_ordering.json';

const GROUP_TABS = [
  {icon: 'emoji_picker:schedule', group: 'history', active: true},
  {icon: 'emoji_picker:insert_emoticon', group: '0', active: false},
  {icon: 'emoji_picker:emoji_people', group: '1', active: false},
  {icon: 'emoji_picker:emoji_nature', group: '2', active: false},
  {icon: 'emoji_picker:emoji_food_beverage', group: '3', active: false},
  {icon: 'emoji_picker:emoji_transportation', group: '4', active: false},
  {icon: 'emoji_picker:emoji_events', group: '5', active: false},
  {icon: 'emoji_picker:emoji_objects', group: '6', active: false},
  {icon: 'emoji_picker:emoji_symbols', group: '7', active: false},
  {icon: 'emoji_picker:flag', group: '8', active: false},
];

/**
 * Constructs the emoji group data structure from a given list of emoji
 * strings. Note: returned emoji have no variants.
 *
 * @param {!Array<Codepoints>} recentEmoji list of recently used emoji strings.
 * @return {!Array<!Emoji>} list of emoji data structures
 */
function makeRecentlyUsed(recentEmoji) {
  return recentEmoji.map(emoji => ({base: emoji, alternates: []}));
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
      emojiDataUrl: {type: String, value: EMOJI_ORDERING_JSON},
      emojiGroups: {type: Array},
      /** @type {?EmojiData} */
      emojiData: {
        type: Object,
        observer: 'onEmojiDataChanged',
      },
      /** @type {EmojiGroup} */
      history: {type: Object},
      search: {type: String},
    };
  }

  constructor() {
    super();

    this.recentEmojiStore = new RecentEmojiStore();

    this.emojiGroups = GROUP_TABS;
    this.emojiData = [];
    this.history = {
      'group': 'Recently Used',
      'emoji': makeRecentlyUsed(this.recentEmojiStore.data),
    };
    this.search = '';

    /** @type {?number} */
    this.scrollTimeout = null;

    /** @type {?EmojiButton} */
    this.activeVariant = null;
  }

  ready() {
    super.ready();

    const xhr = new XMLHttpRequest();
    xhr.onloadend = () => this.onEmojiDataLoaded(xhr.responseText);
    xhr.open('GET', this.emojiDataUrl);
    xhr.send();

    this.addEventListener(
        GROUP_BUTTON_EVENT, ev => this.selectGroup(ev.detail.group));
    this.addEventListener(
        EMOJI_BUTTON_EVENT, ev => this.insertEmoji(ev.detail.emoji));
    this.addEventListener(
        SHOW_VARIANTS_EVENT,
        ev => this.onShowEmojiVariants(/** @type {!ShowVariantsEvent} */ (ev)));
    this.addEventListener('click', () => this.hideEmojiVariants());

    this.updateStyles({
      '--emoji-picker-width': EMOJI_PICKER_WIDTH_PX,
      '--emoji-picker-height': EMOJI_PICKER_HEIGHT_PX,
      '--emoji-size': EMOJI_SIZE_PX,
      '--emoji-per-row': EMOJI_PER_ROW,
    });
  }

  /**
   * @param {!string} emoji
   */
  insertEmoji(emoji) {
    chrome.send('insertEmoji', [emoji]);
    this.recentEmojiStore.bumpEmoji(emoji);
    this.set(
        ['history', 'emoji'], makeRecentlyUsed(this.recentEmojiStore.data));
  }

  /**
   * @param {string} newGroup
   */
  selectGroup(newGroup) {
    // scroll to selected group's element.
    const group =
        this.shadowRoot.querySelector(`div[data-group="${newGroup}"]`);
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
    // get bounding rect of scrollable emoji region.
    const thisRect =
        this.shadowRoot.querySelector('.emoji-groups').getBoundingClientRect();

    const groupElements = Array.from(
        this.shadowRoot.querySelectorAll('.emoji-groups [data-group]'));

    // activate the first group which is visible for at least 10 pixels,
    // i.e. whose bottom edge is at least 10px below the top edge of the
    // scrollable region.
    const activeGroup = groupElements.find(
        el => el.getBoundingClientRect().bottom - thisRect.top >= 10);

    assert(activeGroup, 'no group element was activated');
    const activeGroupId = activeGroup.dataset.group;

    // set active to true for selected group and false for others.
    this.emojiGroups.forEach((g, i) => {
      const isActive = g.group === activeGroupId;
      this.set(['emojiGroups', i, 'active'], isActive);
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
   * @param {!ShowVariantsEvent} ev
   */
  onShowEmojiVariants(ev) {
    this.hideEmojiVariants();
    this.activeVariant = /** @type {EmojiButton} */ (ev.detail.button);
    this.positionEmojiVariants(ev.detail.variants);
  }

  positionEmojiVariants(el) {
    // TODO(crbug.com/1174311): currently positions horizontally within page.
    // ideal UI would be overflowing the bounds of the page.
    // also need to account for vertical positioning.

    // compute width required for the variant popup as: SIZE * columns + 10.
    // SIZE is emoji width in pixels. number of columns is determined by width
    // of variantRows, then one column each for the base emoji and skin tone
    // indicators if present. 10 pixels are added for padding and the shadow.

    // get size of emoji picker
    const pickerRect =
        this.shadowRoot.querySelector('.emoji-picker').getBoundingClientRect();

    // determine how much overflows the right edge of the window.
    const container = el.shadowRoot.getElementById('container');
    const rect = container.getBoundingClientRect();
    const overflowWidth = rect.x + rect.width - pickerRect.width;
    // shift left by overflowWidth rounded up to next multiple of EMOJI_SIZE.
    const shift = EMOJI_SIZE * Math.ceil(overflowWidth / EMOJI_SIZE);
    // negative value means we are already within bounds, so no shift needed.
    container.style.marginLeft = `-${Math.max(shift, 0)}px`;
  }


  onEmojiDataLoaded(data) {
    this.emojiData = /** @type {!EmojiData} */ (JSON.parse(data));
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
      this.dispatchEvent(createCustomEvent(DATA_LOADED_EVENT));
    }
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
