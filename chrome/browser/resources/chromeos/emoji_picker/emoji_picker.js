// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './emoji_group.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EmojiData} from './types.js';

const EMOJI_ORDERING_JSON = '/emoji_ordering.json';

class EmojiPicker extends PolymerElement {
  static get is() {
    return 'emoji-picker';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      emoji: {type: Array},
      /** @type {?EmojiData} */
      emojiData: {type: Object},
      search: {type: String},
    };
  }

  constructor() {
    super();

    const xhr = new XMLHttpRequest();
    xhr.onloadend = () => this.onEmojiDataLoaded(xhr.responseText);
    xhr.open('GET', EMOJI_ORDERING_JSON);
    xhr.send();

    this.emojiData = [];
    this.emoji = loadTimeData.getString('emoji').split(',');
    this.search = '';
  }

  _formatEmojiData(emojiData) {
    return JSON.stringify(emojiData);
  }

  onEmojiDataLoaded(data) {
    this.emojiData = /** @type {!EmojiData} */ (JSON.parse(data));
  }
}

customElements.define(EmojiPicker.is, EmojiPicker);
