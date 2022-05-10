// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_CLEAR_RECENTS_CLICK} from './events.js';
import {CategoryEnum, EmojiVariants} from './types.js';

class EmoticonGroupComponent extends PolymerElement {
  static get is() {
    return 'emoticon-group';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<EmojiVariants>}*/
      data: {type: Array, readonly: true},
      /** @type {boolean} */
      clearable: {type: Boolean, value: false},
      /** @type {boolean} */
      showClearRecents: {type: Boolean, value: false},
      /** @type {string} */
      category: {type: String, value: CategoryEnum.EMOTICON},
    };
  }

  onEmoticonClick(ev) {
    const emoticonString = ev.target.getAttribute('emoticon-string');
    const emoticonName = ev.target.getAttribute('emoticon-name');
    this.dispatchEvent(createCustomEvent(EMOJI_BUTTON_CLICK, {
      text: emoticonString,
      isVariant: false,
      baseEmoji: emoticonString,
      allVariants: [],
      name: emoticonName,
      category: this.category,
    }));
  }

  /**
   * @param {Number} index
   * @returns {string} unique string to distinguish different emoticons within
   *     same group.
   */
  generateEmoticonId(index) {
    return `emoticon-${index}`;
  }

  /**
   * Handles the click event for show-clear button which results
   * in showing "clear recently used" emoticons button.
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
      EMOJI_CLEAR_RECENTS_CLICK, {category: this.category}));
  }
}

customElements.define(EmoticonGroupComponent.is, EmoticonGroupComponent);
