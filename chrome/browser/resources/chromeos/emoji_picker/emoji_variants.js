// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createCustomEvent, EMOJI_VARIANTS_SHOWN} from './events.js';
import {Emoji} from './types.js';

const GENDER_FEMALE = 9792;       // U+2640 FEMALE_SIGN
const SKIN_TONE_MEDIUM = 127997;  // U+1F3FD EMOJI MODIFIER FITZPATRICK TYPE-4
const FAMILY = 128106;            // U+1F46A FAMILY
const COUPLE = 128107;            // U+1F46B MAN AND WOMAN HOLDING HANDS

/**
 * Determines if the given list of variants has any variant which contains
 * the given codepoint.
 * @param {!Array<!Emoji>} variants
 * @param {!number} codepoint
 * @return {boolean}
 */
function hasVariation(variants, codepoint) {
  const codepointString = String.fromCodePoint(codepoint);
  return variants.findIndex(x => x.string.includes(codepointString)) !== -1;
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
      /** @type {!Array<Emoji>} */
      variants: {type: Array, readonly: true},
      /** @private {!Array<!Array<Emoji>>} */
      variantRows: {type: Array},
      /** @private {?string} */
      baseEmoji: {type: Array},
      /** @private {boolean} */
      showSkinTones: {type: Boolean},
      /** @private {boolean} */
      showBaseEmoji: {type: Boolean}
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
    this.showBaseEmoji = isFamily || isTwoPeople;
    this.baseEmoji = this.variants[0].string;
    this.showSkinTones = isTwoPeople;

    // if we are showing a base emoji separately, omit it from the main grid.
    const gridEmoji =
        this.showBaseEmoji ? this.variants.slice(1) : this.variants;
    const rowLengths = this.computeVariantRowLengths(gridEmoji);
    this.variantRows = partitionArray(gridEmoji, rowLengths);

    this.addEventListener(
        'keydown', (ev) => this.onKeyDown(/** @type {!KeyboardEvent} */ (ev)));
  }

  connectedCallback() {
    beforeNextRender(
        this,
        () => this.shadowRoot.querySelector('emoji-button').focusButton());
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

    return [];
  }

  /**
   * @param {!KeyboardEvent} ev
   */
  onKeyDown(ev) {
    if (ev.key !== 'Escape')
      return;

    // hide visible variants when escape is pressed.
    // TODO(crbug.com/1177020): does not work (whole dialog is closed instead).
    ev.preventDefault();
    ev.stopPropagation();
    this.dispatchEvent(createCustomEvent(EMOJI_VARIANTS_SHOWN, {
      button: null,
      variants: null,
    }));
  }
}

customElements.define(EmojiVariants.is, EmojiVariants);
