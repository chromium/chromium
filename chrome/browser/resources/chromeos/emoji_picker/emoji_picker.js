// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './emoji_group.js';
import './emoji_group_button.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, DATA_LOADED_EVENT, EMOJI_BUTTON_EVENT, GROUP_BUTTON_EVENT} from './events.js';
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
