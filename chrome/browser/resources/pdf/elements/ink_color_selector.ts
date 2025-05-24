// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Color} from '../constants.js';
import {hexToColor} from '../pdf_viewer_utils.js';

import {getCss} from './ink_color_selector.css.js';
import {getHtml} from './ink_color_selector.html.js';

export interface ColorOption {
  label: string;
  color: string;
  blended: boolean;
}

/**
 * @returns Whether `lhs` and `rhs` have the same RGB values or not.
 */
function areColorsEqual(lhs: Color, rhs: Color): boolean {
  return lhs.r === rhs.r && lhs.g === rhs.g && lhs.b === rhs.b;
}

const InkColorSelectorElementBase = I18nMixinLit(CrLitElement);

export class InkColorSelectorElement extends InkColorSelectorElementBase {
  static get is() {
    return 'ink-color-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      colors: {type: Array},
      currentColor: {
        notify: true,
        type: Object,
      },
      label: {type: String},
    };
  }

  accessor colors: ColorOption[] = [];
  accessor currentColor: Color = {r: 0, g: 0, b: 0};
  accessor label: string = '';

  protected onColorClick_(e: Event) {
    this.setBrushColor_(e.currentTarget as HTMLInputElement);
  }

  protected onCrGridFocusChanged_(e: CustomEvent<HTMLElement>) {
    this.setBrushColor_(e.detail as HTMLInputElement);
  }

  override focus() {
    const selectedButton =
        this.shadowRoot.querySelector<HTMLElement>('input[checked]');
    assert(selectedButton);
    selectedButton.focus();
  }

  protected getTabIndex_(color: string): number {
    return this.isCurrentColor_(color) ? 0 : -1;
  }

  protected isCurrentColor_(hex: string): boolean {
    return areColorsEqual(this.currentColor, hexToColor(hex));
  }

  protected getBlendedClass_(item: ColorOption): string {
    return item.blended ? 'blended' : '';
  }

  private setBrushColor_(colorButton: HTMLInputElement): void {
    const hex = colorButton.value;
    assert(hex);

    const newColor: Color = hexToColor(hex);
    if (areColorsEqual(this.currentColor, newColor)) {
      return;
    }

    this.currentColor = newColor;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-color-selector': InkColorSelectorElement;
  }
}

customElements.define(InkColorSelectorElement.is, InkColorSelectorElement);
