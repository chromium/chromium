// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_variants.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_button.html.js';
import {createCustomEvent, EMOJI_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN, EmojiButtonClickEvent, EmojiVariantsShownEvent} from './events.js';
import {CategoryEnum, Emoji} from './types.js';

export class EmojiButton extends PolymerElement {
  static get is() {
    return 'emoji-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      emoji: {type: String, readonly: true},
      variants: {type: Array, readonly: true},
      variantsVisible: {type: Boolean, value: false},
      variant: {type: Boolean, value: false, readonly: true},
      disabled: {type: Boolean, value: false, readonly: true},
      base: {type: String},
      allVariants: {type: Array, readonly: true},
      tooltip: {type: String, readonly: true},
      category: {
        type: String,
        value: CategoryEnum.EMOJI,
        readonly: true,
      },
    };
  }
  emoji: string;
  variants?: Emoji[];
  private variantsVisible: boolean;
  private variant: boolean;
  private disabled: boolean;
  private base?: string;
  private allVariants?: Emoji[];
  private tooltip?: string;
  private category: string;


  private onClick(): void {
    if (this.disabled) {
      return;
    }
    this.dispatchEvent(createCustomEvent(EMOJI_BUTTON_CLICK, {
      text: this.emoji,
      isVariant: this.variant,
      baseEmoji: this.base,
      allVariants: this.allVariants ? this.allVariants : this.variants,
      name: this.tooltip,
      category: this.category,
    }));
  }

  private onContextMenu(ev: Event): void {
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
      const variants = this.variantsVisible ?
          // ShadowRoot is guaranteed to exist so ! is safe
          this.shadowRoot!.querySelector('emoji-variants') :
          null;

      this.dispatchEvent(createCustomEvent(
          EMOJI_VARIANTS_SHOWN,
          {owner: this, variants: variants, baseEmoji: this.emoji}));
    });
  }

  /**
   * Hides emoji variants if any is visible.
   */
  hideEmojiVariants(): void {
    /**
     * TODO(b/233130994): Remove the function as part of the component removal.
     * The function is only added to help merging emoji-button into
     * emoji-group to allow removing emoji-button later.
     */
    this.variantsVisible = false;
  }

  private calculateClassName(): string {
    return (this.variants && this.variants.length > 0) ? 'has-variants' : '';
  }

  private getLabel(): string {
    // TODO(crbug/1227852): Just use emoji as the tooltip once ChromeVox can
    // announce them properly.
    const emojiLabel =
        (navigator.languages.some(lang => lang.startsWith('en')) &&
         this.tooltip) ?
        this.tooltip :
        this.emoji;
    return this.variants?.length ? emojiLabel + ' with variants.' : emojiLabel;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiButton.is]: EmojiButton;
  }
  interface HTMLElementEventMap {
    [EMOJI_VARIANTS_SHOWN]: EmojiVariantsShownEvent;
    [EMOJI_BUTTON_CLICK]: EmojiButtonClickEvent;
  }
}


customElements.define(EmojiButton.is, EmojiButton);
