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
      groups: {type: Array},
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

    this.groups = GROUP_TABS;
    this.emojiData = [];
    this.history = {
      'group': 'Recently Used',
      'emoji': makeRecentlyUsed(this.recentEmojiStore.data),
    };
    this.search = '';
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
   * @param {string} newGroup
   */
  selectGroup(newGroup) {
    let activeGroup = null;
    // set active to true for selected group and false for others.
    this.groups.forEach((g, i) => {
      const isActive = g.group === newGroup;
      this.set(['groups', i, 'active'], isActive);
      if (isActive) {
        activeGroup = g;
      }
    });
    assert(activeGroup, 'no group button was activated');
    // scroll to selected group's element.
    this.shadowRoot.getElementById(`group-${activeGroup.group}`)
        .scrollIntoView();
  }

  /**
   * @param {string} emoji
   */
  insertEmoji(emoji) {
    chrome.send('insertEmoji', [emoji]);
    this.recentEmojiStore.bumpEmoji(emoji);
    this.set(
        ['history', 'emoji'], makeRecentlyUsed(this.recentEmojiStore.data));
  }

  _formatEmojiData(emojiData) {
    return JSON.stringify(emojiData);
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
