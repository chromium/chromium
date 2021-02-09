// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Codepoints} from './types.js';

const GENDER_FEMALE = 9792;       // U+2640 FEMALE_SIGN
const SKIN_TONE_MEDIUM = 127997;  // U+1F3FD EMOJI MODIFIER FITZPATRICK TYPE-4
const FAMILY = 128106;            // U+1F46A FAMILY
const COUPLE = 128107;            // U+1F46B MAN AND WOMAN HOLDING HANDS

/**
 * Determines if the given list of variants has any variant which contains
 * the given codepoint.
 * @param {!Array<Codepoints>} variants
 * @param {!number} codepoint
 * @return {boolean}
 */
function hasVariation(variants, codepoint) {
  return variants.findIndex(x => x.includes(codepoint)) !== -1;
}


/**
 * Partitions source array into array of arrays, where each subarray's
 * length is determined by the corresponding value of subarrayLengths.
 * A negative length indicates skip that many items.
 * @param {!Array<T>} array source array.
 * @param {!Array<number>} subarrayLengths lengths to partition.
 * @return {!Array<!Array<T>>} partitioned array.
 * @template T array item type.
 */
function partitionArray(array, subarrayLengths) {
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

export class EmojiVariants extends PolymerElement {
  static get is() {
    return 'emoji-variants';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<Codepoints>} */
      variants: {type: Array, readonly: true},
      /** @private {!Array<!Array<Codepoints>>} */
      variantRows: {type: Array},
      /** @private {?Codepoints} */
      baseEmoji: {type: Array},
      /** @private {boolean} */
      showSkinTones: {type: Boolean},
    };
  }

  constructor() {
    super();
  }

  ready() {
    super.ready();

    // family picker is basic 5x5 grid.
    const isFamily = this.variants.length === 26 && this.variants[0] == FAMILY;
    // two people is 5x5 grid with 5 skin tones per person.
    const isTwoPeople = this.variants.length === 26 &&
        hasVariation(this.variants, SKIN_TONE_MEDIUM);

    if (isFamily || isTwoPeople) {
      // for these cases, the first variant is the generic one.
      this.baseEmoji = this.variants[0];
    } else {
      this.baseEmoji = null;
    }
    this.showSkinTones = isTwoPeople;

    // if we are showing a base emoji separately, omit it from the main grid.
    const gridEmoji = this.baseEmoji ? this.variants.slice(1) : this.variants;
    const rowLengths = this.computeVariantRowLengths(gridEmoji);
    this.variantRows = partitionArray(gridEmoji, rowLengths);
  }

  computeVariantRowLengths(variants) {
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

    console.error('unimplemented variation: ', variants);
    return [];
  }
}

customElements.define(EmojiVariants.is, EmojiVariants);
