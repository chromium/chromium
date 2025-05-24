// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import {getCss} from './ink_size_selector.css.js';
import {getHtml} from './ink_size_selector.html.js';

export interface SizeOption {
  icon: string;
  label: string;
  size: number;
}

// LINT.IfChange(PenSizes)
export const PEN_SIZES: SizeOption[] = [
  {icon: 'pen-size-1', label: 'ink2BrushSizeExtraThin', size: 1},
  {icon: 'pen-size-2', label: 'ink2BrushSizeThin', size: 2},
  {icon: 'pen-size-3', label: 'ink2BrushSizeMedium', size: 3},
  {icon: 'pen-size-4', label: 'ink2BrushSizeThick', size: 6},
  {icon: 'pen-size-5', label: 'ink2BrushSizeExtraThick', size: 8},
];
// LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:PenSizes)

export const HIGHLIGHTER_SIZES: SizeOption[] = [
  // LINT.IfChange(HighlighterSizes)
  {icon: 'highlighter-size-1', label: 'ink2BrushSizeExtraThin', size: 4},
  {icon: 'highlighter-size-2', label: 'ink2BrushSizeThin', size: 6},
  {icon: 'highlighter-size-3', label: 'ink2BrushSizeMedium', size: 8},
  {icon: 'highlighter-size-4', label: 'ink2BrushSizeThick', size: 12},
  {icon: 'highlighter-size-5', label: 'ink2BrushSizeExtraThick', size: 16},
  // LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:HighlighterSizes)
];

const InkSizeSelectorElementBase = I18nMixinLit(CrLitElement);

export class InkSizeSelectorElement extends InkSizeSelectorElementBase {
  static get is() {
    return 'ink-size-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentSize: {
        notify: true,
        type: Number,
      },
      currentType: {type: String},
    };
  }

  override focus() {
    const selectedButton = this.shadowRoot.querySelector<HTMLElement>(
        'selectable-icon-button[checked]');
    assert(selectedButton);
    selectedButton.focus();
  }

  accessor currentSize: number = 0;
  accessor currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected currentSizeString_(): string {
    return this.currentSize.toString();
  }

  protected onSelectedChanged_(e: CustomEvent<{value: string}>) {
    this.currentSize = Number(e.detail.value);
  }

  protected getCurrentBrushSizes_(): SizeOption[] {
    assert(this.currentType !== AnnotationBrushType.ERASER);
    return this.currentType === AnnotationBrushType.HIGHLIGHTER ?
        HIGHLIGHTER_SIZES :
        PEN_SIZES;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-size-selector': InkSizeSelectorElement;
  }
}

customElements.define(InkSizeSelectorElement.is, InkSizeSelectorElement);
