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

// LINT.IfChange(PenAndEraserSizes)
export const ERASER_SIZES: SizeOption[] = [
  {icon: 'eraser-size-1', label: 'ink2BrushSizeExtraThin', size: 1},
  {icon: 'eraser-size-2', label: 'ink2BrushSizeThin', size: 2},
  {icon: 'eraser-size-3', label: 'ink2BrushSizeMedium', size: 3},
  {icon: 'eraser-size-4', label: 'ink2BrushSizeThick', size: 6},
  {icon: 'eraser-size-5', label: 'ink2BrushSizeExtraThick', size: 8},
];

export const PEN_SIZES: SizeOption[] = [
  {icon: 'pen-size-1', label: 'ink2BrushSizeExtraThin', size: 1},
  {icon: 'pen-size-2', label: 'ink2BrushSizeThin', size: 2},
  {icon: 'pen-size-3', label: 'ink2BrushSizeMedium', size: 3},
  {icon: 'pen-size-4', label: 'ink2BrushSizeThick', size: 6},
  {icon: 'pen-size-5', label: 'ink2BrushSizeExtraThick', size: 8},
];
// LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:PenAndEraserSizes)

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

  currentSize: number = 0;
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected isCurrentSize_(size: number): boolean {
    return this.currentSize === size;
  }

  protected getTabIndexForSize_(size: number): number {
    return this.isCurrentSize_(size) ? 0 : -1;
  }

  protected onSizeClick_(e: Event) {
    this.setBrushSize_(e.currentTarget as HTMLElement);
  }

  protected onSizeKeydown_(e: KeyboardEvent) {
    // Only handle arrow keys.
    const isPrevious = e.key === 'ArrowLeft' || e.key === 'ArrowUp';
    const isNext = e.key === 'ArrowRight' || e.key === 'ArrowDown';
    if (!isPrevious && !isNext) {
      return;
    }
    e.preventDefault();

    const currSizeButton = e.target as HTMLElement;
    const currentIndex = Number(currSizeButton.dataset['index']);

    const brushSizes = this.getCurrentBrushSizes_();
    const numOptions = brushSizes.length;
    const delta = isNext ? 1 : -1;
    const newIndex = (numOptions + currentIndex + delta) % numOptions;

    const newSize = brushSizes[newIndex]!.size;
    const newSizeButton =
        this.shadowRoot!.querySelector<HTMLElement>(`[data-size='${newSize}']`);
    assert(newSizeButton);
    this.setBrushSize_(newSizeButton);
    newSizeButton.focus();
  }

  protected getCurrentBrushSizes_(): SizeOption[] {
    switch (this.currentType) {
      case AnnotationBrushType.ERASER:
        return ERASER_SIZES;
      case AnnotationBrushType.HIGHLIGHTER:
        return HIGHLIGHTER_SIZES;
      case AnnotationBrushType.PEN:
        return PEN_SIZES;
    }
  }

  private setBrushSize_(sizeButton: HTMLElement): void {
    const size = Number(sizeButton.dataset['size']);
    if (this.currentSize === size) {
      return;
    }

    this.currentSize = size;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-size-selector': InkSizeSelectorElement;
  }
}

customElements.define(InkSizeSelectorElement.is, InkSizeSelectorElement);
