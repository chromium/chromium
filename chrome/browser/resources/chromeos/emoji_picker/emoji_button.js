// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_variants.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN} from './events.js';
import {CategoryEnum, Emoji} from './types.js';

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
      /** @type {?Array<Emoji>} */
      variants: {type: Array, readonly: true},
      /** @type {!boolean} */
      variantsVisible: {type: Boolean, value: false},
      /** @type {!Boolean} */
      variant: {type: Boolean, value: false, readonly: true},
      /** @type {!boolean} */
      disabled: {type: Boolean, value: false, readonly: true},
      /** @type {!string} */
      base: {type: String},
      /** @type {?Array<Emoji>} */
      allVariants: {type: Array, readonly: true},
      /** @type {!string} */
      tooltip: {type: String, readonly: true},
    };
  }

  constructor() {
    super();
  }

  getButton() {
    return this.$['emoji-button'];
  }

  focusButton(options) {
    this.$['emoji-button'].focus(options);
  }

  onClick(ev) {
    if (this.disabled) {
      return;
    }
    this.dispatchEvent(createCustomEvent(EMOJI_BUTTON_CLICK, {
      text: this.emoji,
      isVariant: this.variant,
      baseEmoji: this.base,
      allVariants: this.allVariants ? this.allVariants : this.variants,
      name: this.tooltip,
      category: CategoryEnum.EMOJI,
    }));
  }

  onContextMenu(ev) {
    ev.preventDefault();

    if (this.disabled) {
      return;
    }

    if (this.variants && this.variants.length) {
      this.variantsVisible = !this.variantsVisible;
    }

    // send event so emoji-picker knows to close other variants.
    // need to defer this until <emoji-variants> is created and sized by
    // Polymer.
    beforeNextRender(this, () => {
      const button = this.variantsVisible ? this : null;
      const variants = this.variantsVisible ?
          this.shadowRoot.querySelector('emoji-variants') :
          null;

      this.dispatchEvent(
          createCustomEvent(EMOJI_VARIANTS_SHOWN, {button, variants}));
    });
  }

  _className(variants) {
    return variants && variants.length > 0 ? 'has-variants' : '';
  }

  _label(tooltip, emoji, variants) {
    // TODO(crbug/1227852): Just use emoji as the tooltip once ChromeVox can
    // announce them properly.
    const emojiLabel =
        navigator.languages.some(lang => lang.startsWith('en')) > 0 ? tooltip :
                                                                      emoji;
    return variants && variants.length ? emojiLabel + ' with variants.' :
                                         emojiLabel;
  }
}

customElements.define(EmojiButton.is, EmojiButton);
