// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, EMOJI_CLEAR_RECENTS_CLICK} from './events.js';
import {CategoryEnum, EmojiVariants} from './types.js';

class EmojiGroupComponent extends PolymerElement {
  static get is() {
    return 'emoji-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<EmojiVariants>} */
      data: {type: Array, readonly: true},
      /** @type {Object<string,string>} */
      preferred: {type: Object},
      /** @type {boolean} */
      clearable: {type: Boolean, value: false},
      /** @type {boolean} */
      showClearRecents: {type: Boolean, value: false},
      /** @type {string} */
      category: {type: String, value: CategoryEnum.EMOJI},
    };
  }

  /** @param emoji {Emoji} */
  getTooltipForEmoji(emoji) {
    return emoji.name;
  }

  getDisplayEmojiForEmoji(emoji) {
    return this.preferred[emoji] || emoji;
  }

  /**
   * Handles the click event for show-clear button which results
   * in showing "clear recently used emojis" button.
   *
   * @param {Event} ev
   */
  onClearClick(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = true;
  }

  /**
   * Handles the event for clicking on the "clear recently used" button.
   * It makes "show-clear" button disappear and fires an event
   * indicating that the "clear recently used" is clicked.
   *
   * @fires CustomEvent#`EMOJI_CLEAR_RECENTS_CLICK`
   * @param {Event} ev
   */
  onClearRecentsClick(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = false;
    this.dispatchEvent(createCustomEvent(
      EMOJI_CLEAR_RECENTS_CLICK,  {category: this.category}));
  }
}

customElements.define(EmojiGroupComponent.is, EmojiGroupComponent);
