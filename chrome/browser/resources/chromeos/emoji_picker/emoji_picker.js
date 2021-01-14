// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.js';
import './emoji_group.js';
import './emoji_group_button.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GROUP_BUTTON_EVENT} from './events.js';
import {EmojiData, EmojiGroup} from './types.js';

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

class EmojiPicker extends PolymerElement {
  static get is() {
    return 'emoji-picker';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      groups: {type: Array},
      /** @type {?EmojiData} */
      emojiData: {type: Object},
      /** @type {EmojiGroup} */
      history: {type: Object},
      search: {type: String},
    };
  }

  constructor() {
    super();

    this.groups = GROUP_TABS;
    this.emojiData = [];
    // TODO(https://crbug.com/1164828): replace placeholder frequently used
    // data.
    this.history = {
      'group': 'Frequently used',
      'emoji': [
        {
          'base': [128512],
          'alternates': [],
        },
        {
          'base': [128513],
          'alternates': [],
        },
      ],
    };
    this.search = '';
  }

  ready() {
    super.ready();

    const xhr = new XMLHttpRequest();
    xhr.onloadend = () => this.onEmojiDataLoaded(xhr.responseText);
    xhr.open('GET', EMOJI_ORDERING_JSON);
    xhr.send();

    this.addEventListener(
        GROUP_BUTTON_EVENT, ev => this.selectGroup(ev.detail.group));
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

  _formatEmojiData(emojiData) {
    return JSON.stringify(emojiData);
  }

  onEmojiDataLoaded(data) {
    this.emojiData = /** @type {!EmojiData} */ (JSON.parse(data));
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
