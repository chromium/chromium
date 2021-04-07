// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, EMOJI_CLEAR_RECENTS_CLICK} from './events.js';
import {EmojiGroup} from './types.js';

class EmojiGroupComponent extends PolymerElement {
  static get is() {
    return 'emoji-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {EmojiGroup} */
      data: {type: Object, readonly: true},
      /** @type {Object<string,string>} */
      preferred: {type: Object},
      /** @type {boolean} */
      clearable: {type: Boolean, value: false},
      /** @type {boolean} */
      showClearRecents: {type: Boolean, value: false},
    };
  }

  constructor() {
    super();
  }

  getDisplayEmojiForEmoji(emoji) {
    return this.preferred[emoji] || emoji;
  }

  onClearClick(ev) {
    ev.preventDefault();
    this.showClearRecents = true;
    console.error('clear clicked');
  }

  onClearRecentsClick(ev) {
    ev.preventDefault();
    this.showClearRecents = false;
    this.dispatchEvent(createCustomEvent(EMOJI_CLEAR_RECENTS_CLICK, {}));
  }
}

customElements.define(EmojiGroupComponent.is, EmojiGroupComponent);
