// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './emoji_button.js';

import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './emoji_variants.html.js';
import {createCustomEvent, EMOJI_VARIANTS_SHOWN} from './events.js';
import {Emoji} from './types.js';

const SKIN_TONE_MEDIUM = '🏽';  // U+1F3FD EMOJI MODIFIER FITZPATRICK TYPE-4
const FAMILY = '👪';               // U+1F46A FAMILY

/**
 * Determines if the given list of variants has any variant which contains
 * the given codepoint.
 */
function hasVariation(variants: Emoji[], codepoint: string): boolean {
  return variants.findIndex(x => x.string?.includes(codepoint)) !== -1;
}


/**
 * Partitions source array into array of arrays, where each subarray's
 * length is determined by the corresponding value of subarrayLengths.
 * A negative length indicates skip that many items.
 */
function partitionArray<T>(array: T[], subarrayLengths: number[]): T[][] {
  const subarrays = [];
  let used = 0;
  for (const len of subarrayLengths) {
    if (len < 0) {
      used += -len;
      continue;
    }
    subarrays.push(array.slice(used, used + len));
    used += len;
  }
  return subarrays;
}

export interface EmojiVariants {
  $: {
    fakeFocusTarget: HTMLElement,
  };
}

export class EmojiVariants extends PolymerElement {
  static get is() {
    return 'emoji-variants' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      variants: {type: Array, readonly: true},
      groupedTone: {type: Boolean, readonly: true},
      groupedGender: {type: Boolean, readonly: true},
      baseEmoji: {type: Array},
      showSkinTones: {type: Boolean},
      showBaseEmoji: {type: Boolean},
      tooltip: {type: String},
    };
  }
  variants: Emoji[];
  private groupedTone = false;
  private groupedGender = false;
  private baseEmoji: string;
  private showSkinTones: boolean;
  private showBaseEmoji: boolean;
  private tooltip: string;

  override ready() {
    super.ready();

    // family picker is basic 5x5 grid.
    const isFamily =
        this.variants.length === 26 && this.variants[0]?.string === FAMILY;
    // two people is 5x5 grid with 5 skin tones per person.
    const isTwoPeople = this.variants.length === 26 &&
        hasVariation(this.variants, SKIN_TONE_MEDIUM);
    this.showBaseEmoji = isFamily || isTwoPeople;
    this.baseEmoji = this.variants[0]?.string ?? '';
    this.showSkinTones = isTwoPeople;

    this.addEventListener('keydown', (ev) => this.onKeyDown(ev));
  }

  override connectedCallback() {
    beforeNextRender(this, () => this.$.fakeFocusTarget.focus());
  }

  private computeVariantRows(showBaseEmoji: boolean, variants: Emoji[]):
      Emoji[][] {
    // if we are showing a base emoji separately, omit it from the main grid.
    const gridEmoji = showBaseEmoji ? variants.slice(1) : variants;
    const rowLengths = this.computeVariantRowLengths(gridEmoji);
    return partitionArray(gridEmoji, rowLengths);
  }

  private computeVariantRowLengths(variants: Emoji[]): number[] {
    if (!variants.length) {
      return [];
    }

    if (variants.length <= 6) {
      // one row of gender or skin tone variants.
      return [variants.length];
    } else if (variants.length === 18) {
      // three rows of skin tone and gender variants.
      return [6, 6, 6];
    } else if (variants.length === 25) {
      // 5x5 grid of family or skin tone options.
      return [5, 5, 5, 5, 5];
    }

    return [];
  }

  private onKeyDown(ev: KeyboardEvent): void {
    if (ev.key !== 'Escape') {
      return;
    }

    // hide visible variants when escape is pressed.
    // TODO(crbug.com/1177020): does not work (whole dialog is closed instead).
    ev.preventDefault();
    ev.stopPropagation();
    this.dispatchEvent(createCustomEvent(EMOJI_VARIANTS_SHOWN, {
      baseEmoji: this.baseEmoji,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiVariants.is]: EmojiVariants;
  }
}


customElements.define(EmojiVariants.is, EmojiVariants);
