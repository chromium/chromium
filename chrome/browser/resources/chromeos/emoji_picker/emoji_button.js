// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_variants.js';

import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN} from './events.js';
import {Emoji} from './types.js';

export class EmojiButton extends PolymerElement {
  static get is() {
    return 'emoji-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!string} */
      emoji: {type: String, readonly: true},
      /** @type {Array<Emoji>} */
      variants: {type: Array, readonly: true},
      /** @type {!boolean} */
      variantsVisible: {type: Boolean, value: false},
      /** @type {!boolean} */
      disabled: {type: Boolean, value: false, readonly: true},
    };
  }

  constructor() {
    super();
  }

  onClick(ev) {
    if (this.disabled)
      return;
    this.dispatchEvent(
        createCustomEvent(EMOJI_BUTTON_CLICK, {emoji: this.emoji}));
  }

  onContextMenu(ev) {
    ev.preventDefault();  // disable standard context menu
  }

  /**
   * @param {!MouseEvent} ev
   */
  onMouseUp(ev) {
    // only handle right mouse button clicks.
    if (this.disabled || ev.button !== 2)
      return;
    if (this.variants && this.variants.length) {
      this.variantsVisible = !this.variantsVisible;
      if (this.variantsVisible) {
        // need to defer this until <emoji-variants> is created and sized by
        // Polymer.
        beforeNextRender(this, () => {
          this.dispatchEvent(createCustomEvent(EMOJI_VARIANTS_SHOWN, {
            button: this,
            variants: this.shadowRoot.querySelector('emoji-variants'),
          }));
        });
      }
    }
    ev.preventDefault();
  }

  _className(variants) {
    return variants && variants.length > 0 ? 'has-variants' : '';
  }
}

customElements.define(EmojiButton.is, EmojiButton);
