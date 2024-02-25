// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/264211466): This is only used by emoji_variants.html. The only props
// that are set seem to be emoji, variant, base, all-variants, tooltip  and
// disabled.  I can delete the others in a follow up, but not changing this file
// to make the revert simpler to review.
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/emoji_picker/emoji_variants.html
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_button.html.js';
import {createCustomEvent, EMOJI_IMG_BUTTON_CLICK, EMOJI_TEXT_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN, EmojiImgButtonClickEvent, EmojiTextButtonClickEvent, EmojiVariantsShownEvent} from './events.js';
import {CategoryEnum, Emoji, Gender, Tone} from './types.js';

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
      variant: {type: Boolean, value: false, readonly: true},
      tone: {type: Number, readonly: true},
      gender: {type: Number, readonly: true},
      groupedTone: {type: Boolean, readonly: true},
      groupedGender: {type: Boolean, readonly: true},
      disabled: {type: Boolean, value: false, readonly: true},
      base: {type: String},
      allVariants: {type: Array, readonly: true},
      tooltip: {type: String, readonly: true},
    };
  }
  emoji: string;
  private variant: boolean;
  private tone?: Tone;
  private gender?: Gender;
  private groupedTone = false;
  private groupedGender = false;
  private disabled: boolean;
  private base?: string;
  private allVariants?: Emoji[];
  private tooltip?: string;


  private onClick(): void {
    if (this.disabled) {
      return;
    }

    this.dispatchEvent(createCustomEvent(EMOJI_TEXT_BUTTON_CLICK, {
      name: this.tooltip,
      category: CategoryEnum.EMOJI,
      text: this.emoji,
      baseEmoji: this.base,
      isVariant: this.variant,
      tone: this.tone,
      gender: this.gender,
      groupedTone: this.groupedTone,
      groupedGender: this.groupedGender,
      alternates: this.allVariants ?? [],
    }));
  }

  private getLabel(): string {
    // TODO(crbug/1227852): Just use emoji as the tooltip once ChromeVox can
    // announce them properly.
    return (navigator.languages.some(lang => lang.startsWith('en')) &&
            this.tooltip) ?
        this.tooltip :
        this.emoji;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiButton.is]: EmojiButton;
  }
  interface HTMLElementEventMap {
    [EMOJI_VARIANTS_SHOWN]: EmojiVariantsShownEvent;
    [EMOJI_TEXT_BUTTON_CLICK]: EmojiTextButtonClickEvent;
    [EMOJI_IMG_BUTTON_CLICK]: EmojiImgButtonClickEvent;
  }
}


customElements.define(EmojiButton.is, EmojiButton);
