// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, EMOJI_BUTTON_EVENT} from './events.js';
import {Codepoints} from './types.js';

export class EmojiButton extends PolymerElement {
  static get is() {
    return 'emoji-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Codepoints} */
      emoji: {type: Array},
      /** @type {!Array<Codepoints>} */
      variants: {type: Array},
    };
  }

  constructor() {
    super();
  }

  onClick(ev) {
    this.dispatchEvent(createCustomEvent(
        EMOJI_BUTTON_EVENT, {emoji: this._renderEmoji(this.emoji)}));
  }

  _className(variants) {
    return variants && variants.length > 0 ? 'has-variants' : '';
  }

  /**
   * @param {Codepoints} codepoints
   */
  _renderEmoji(codepoints) {
    return String.fromCodePoint(...codepoints);
  }
}

customElements.define(EmojiButton.is, EmojiButton);
