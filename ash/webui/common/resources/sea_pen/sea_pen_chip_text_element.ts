// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays Sea Pen chip text.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {assert} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sea_pen_chip_text_element.html.js';

export interface SeaPenChipTextElement {
  $: {
    underline: HTMLDivElement,
    chipText: HTMLDivElement,
  };
}

export class SeaPenChipTextElement extends PolymerElement {
  static get is() {
    return 'sea-pen-chip-text';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      textAnimationEnabled: Boolean,

      chipText: {
        type: Object,
        observer: 'onChipTextChanged_',
      },
    };
  }

  textAnimationEnabled: boolean;
  chipText: string;

  // gets an array of <span> elements storing the letters of `chipElement`
  // innerHTML.
  private getLettersAsElements_(chipElement: HTMLElement): HTMLElement[] {
    const content = chipElement.innerHTML;
    chipElement.innerHTML = window.trustedTypes!.emptyHTML;
    const letters: HTMLElement[] = [];
    // splits the chip text value into graphemes and create a <span> element
    // under chip element to hold each grapheme.
    const segmenter = new Intl.Segmenter(
        Intl.DateTimeFormat().resolvedOptions().locale,
        {granularity: 'grapheme'});
    for (const {segment} of segmenter.segment(content)) {
      const letter = document.createElement('span');
      letter.className = 'letter';
      letter.innerHTML = sanitizeInnerHtml(segment);
      chipElement.appendChild(letter);
      letters.push(letter);
    }
    return letters;
  }

  // removes the first `n` letter elements of #chipText node.
  private removeLetterElementsFromChip_(n: number) {
    const chip = this.$.chipText;
    for (let i = 0; i < n; i++) {
      if (!chip.firstChild) {
        return;
      }
      chip.removeChild(chip.firstChild);
    }
  }

  // animates the underline width from `oldWidth` value to `newWidth` value in
  // `duration` ms.
  private animateUnderlineWidthChange_(
      oldWidth: number, newWidth: number, duration: number) {
    const underline = this.$.underline;
    assert(!!underline, 'underline element should be available');
    underline.animate(
        {
          width: [`${oldWidth}px`, `${newWidth}px`],
        },
        {
          duration: duration,
          easing: 'cubic-bezier(0.00, 0.00, 0.00, 1.00)',
        });
  }

  private animateLetterOut_(letterElements: HTMLElement[], i: number) {
    // The delay the animation between letters is 17ms.
    setTimeout(() => {
      letterElements[i].className = 'letter out';
    }, i * 17);
  }

  private animateLetterIn_(letterElements: HTMLElement[], i: number) {
    // The delay the animation between letters is 17ms.
    setTimeout(function() {
      letterElements[i].className = 'letter in';
    }, i * 17);
  }

  private changeChipText_(chipElement: HTMLElement, newText: string) {
    assert(!!chipElement);
    const currentChipWidth = chipElement.clientWidth;
    const currentLetterElements = this.getLettersAsElements_(chipElement!);
    // Animates the letters of old chip text out.
    for (let i = 0; i < currentLetterElements.length; i++) {
      this.animateLetterOut_(currentLetterElements, i);
    }

    // The animation of the new chip text is delayed 200ms from the old chip
    // value animation.
    setTimeout(() => {
      this.removeLetterElementsFromChip_(currentLetterElements.length);
      chipElement!.innerHTML = sanitizeInnerHtml(newText);
      const newLetterElements = this.getLettersAsElements_(chipElement);
      const newChipWidth = chipElement.clientWidth;

      // Width transition of the underline from the width value of the old chip
      // text to the width value of the new chip text.
      this.animateUnderlineWidthChange_(
          currentChipWidth, newChipWidth, 250 + newLetterElements.length * 17);

      for (let i = 0; i < newLetterElements.length; i++) {
        this.animateLetterIn_(newLetterElements, i);
      }
    }, 200 + currentLetterElements.length * 17);
  }

  private onChipTextChanged_(newText: string, oldText: string) {
    const chip = this.$.chipText;
    assert(!!chip);
    if (!this.textAnimationEnabled) {
      // In case that the template is switched when a chip is being selected,
      // the chip still contains all the <span> children of its letters, needs
      // to clear these children and update the chip text otherwise it will show
      // the chip option of the old template instead of updating to the new chip
      // option of the new template.
      if (chip.childElementCount > 0) {
        this.removeLetterElementsFromChip_(chip.childElementCount);
      }
      // Update the chip innerHTML with the new chip text value if its value is
      // not automatically updated.
      if (chip!.innerHTML !== newText) {
        chip!.innerHTML = sanitizeInnerHtml(newText);
      }
      return;
    }
    chip!.innerHTML = sanitizeInnerHtml(oldText);
    this.changeChipText_(chip!, newText);
  }
}

customElements.define(SeaPenChipTextElement.is, SeaPenChipTextElement);
