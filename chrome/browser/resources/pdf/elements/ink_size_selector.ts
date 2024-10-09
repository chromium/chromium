// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import {getCss} from './ink_size_selector.css.js';
import {getHtml} from './ink_size_selector.html.js';

interface SizeOption {
  icon: string;
  name: string;
  size: number;
}

// TODO(crbug.com/341282609): Choose production size values. Add labels.
export const ERASER_SIZES: SizeOption[] = [
  {icon: 'eraser-size-1', name: 'sizeExtraThin', size: 1},
  {icon: 'eraser-size-2', name: 'sizeThin', size: 2},
  {icon: 'eraser-size-3', name: 'sizeExtraMedium', size: 3},
  {icon: 'eraser-size-4', name: 'sizeThick', size: 6},
  {icon: 'eraser-size-5', name: 'sizeExtraThick', size: 8},
];

export const HIGHLIGHTER_SIZES: SizeOption[] = [
  {icon: 'highlighter-size-1', name: 'sizeExtraThin', size: 4},
  {icon: 'highlighter-size-2', name: 'sizeThin', size: 6},
  {icon: 'highlighter-size-3', name: 'sizeExtraMedium', size: 8},
  {icon: 'highlighter-size-4', name: 'sizeThick', size: 12},
  {icon: 'highlighter-size-5', name: 'sizeExtraThick', size: 16},
];

export const PEN_SIZES: SizeOption[] = [
  {icon: 'pen-size-1', name: 'sizeExtraThin', size: 1},
  {icon: 'pen-size-2', name: 'sizeThin', size: 2},
  {icon: 'pen-size-3', name: 'sizeExtraMedium', size: 3},
  {icon: 'pen-size-4', name: 'sizeThick', size: 6},
  {icon: 'pen-size-5', name: 'sizeExtraThick', size: 8},
];

export class InkSizeSelectorElement extends CrLitElement {
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

  currentSize: number = PEN_SIZES[2]!.size;
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected isCurrentSize_(size: number): boolean {
    return this.currentSize === size;
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
