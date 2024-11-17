// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';
import './ink_size_selector.js';
import './viewer_bottom_toolbar_dropdown.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {Color} from '../constants.js';
import {record, UserAction} from '../metrics.js';
import {blendHighlighterColorValue, colorToHex} from '../pdf_viewer_utils.js';

import {ERASER_SIZES, HIGHLIGHTER_SIZES, PEN_SIZES} from './ink_size_selector.js';
import type {SizeOption} from './ink_size_selector.js';
import {getCss} from './viewer_bottom_toolbar.css.js';
import {getHtml} from './viewer_bottom_toolbar.html.js';
import type {ViewerBottomToolbarDropdownElement} from './viewer_bottom_toolbar_dropdown.js';

export interface ViewerBottomToolbarElement {
  $: {
    size: ViewerBottomToolbarDropdownElement,
  };
}

export class ViewerBottomToolbarElement extends CrLitElement {
  static get is() {
    return 'viewer-bottom-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentColor: {type: Object},
      currentSize: {type: Number},
      currentType: {type: String},
    };
  }

  constructor() {
    super();
    record(UserAction.OPEN_INK2_BOTTOM_TOOLBAR);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('currentColor') && this.currentColor) {
      this.onCurrentColorUpdated_();
    }
  }

  currentColor?: Color;
  currentSize: number = 0;
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;

  protected getSizeIcon_(): string {
    let options: SizeOption[];
    switch (this.currentType) {
      case AnnotationBrushType.ERASER:
        options = ERASER_SIZES;
        break;
      case AnnotationBrushType.HIGHLIGHTER:
        options = HIGHLIGHTER_SIZES;
        break;
      case AnnotationBrushType.PEN:
        options = PEN_SIZES;
        break;
      default:
        assertNotReached();
    }
    assert(options);

    const option = options.find(option => option.size === this.currentSize);
    assert(option);

    return 'pdf:' + option.icon;
  }

  private onCurrentColorUpdated_(): void {
    assert(this.currentColor);

    const color = this.currentType === AnnotationBrushType.HIGHLIGHTER ?
        {
          r: blendHighlighterColorValue(this.currentColor.r),
          g: blendHighlighterColorValue(this.currentColor.g),
          b: blendHighlighterColorValue(this.currentColor.b),
        } :
        this.currentColor;

    this.style.setProperty('--ink-brush-color', colorToHex(color));
  }

  protected shouldShowColorOptions_(): boolean {
    return this.currentType !== AnnotationBrushType.ERASER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-bottom-toolbar': ViewerBottomToolbarElement;
  }
}

customElements.define(
    ViewerBottomToolbarElement.is, ViewerBottomToolbarElement);
