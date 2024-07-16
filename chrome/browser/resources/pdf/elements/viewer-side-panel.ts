// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush} from '../constants.js';
import {PluginController} from '../controller.js';

import {getHtml} from './viewer-side-panel.html.js';

interface SizeOption {
  name: string;
  size: number;
}

// TODO(crbug.com/341282609): Choose production size values. Add icons and
// labels.
const ERASER_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 1},
  {name: 'sizeThin', size: 2},
  {name: 'sizeExtraMedium', size: 3},
  {name: 'sizeThick', size: 6},
  {name: 'sizeExtraThick', size: 8},
];

const HIGHLIGHTER_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 4},
  {name: 'sizeThin', size: 6},
  {name: 'sizeExtraMedium', size: 8},
  {name: 'sizeThick', size: 12},
  {name: 'sizeExtraThick', size: 16},
];

const PEN_SIZES: SizeOption[] = [
  {name: 'sizeExtraThin', size: 1},
  {name: 'sizeThin', size: 2},
  {name: 'sizeExtraMedium', size: 3},
  {name: 'sizeThick', size: 6},
  {name: 'sizeExtraThick', size: 8},
];

export interface ViewerSidePanelElement {
  $: {
    eraser: HTMLElement,
    highlighter: HTMLElement,
    pen: HTMLElement,
  };
}

export class ViewerSidePanelElement extends CrLitElement {
  static get is() {
    return 'viewer-side-panel';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      brushDirty_: {type: Boolean},
      currentType_: {type: String},
    };
  }

  // Indicates the brush has changes and should be updated in
  // `this.pluginController_`.
  private brushDirty_: boolean = true;
  private currentType_: AnnotationBrushType = AnnotationBrushType.PEN;

  private brushes_: Map<AnnotationBrushType, AnnotationBrush>;
  private pluginController_: PluginController = PluginController.getInstance();

  constructor() {
    super();

    // Default brushes.
    this.brushes_ = new Map([
      [
        AnnotationBrushType.ERASER,
        {
          type: AnnotationBrushType.ERASER,
          size: ERASER_SIZES[2].size,
        },
      ],
      [
        AnnotationBrushType.HIGHLIGHTER,
        {
          type: AnnotationBrushType.HIGHLIGHTER,
          color: {r: 0, g: 0, b: 0},
          size: HIGHLIGHTER_SIZES[2].size,
        },
      ],
      [
        AnnotationBrushType.PEN,
        {
          type: AnnotationBrushType.PEN,
          color: {r: 0, g: 0, b: 0},
          size: PEN_SIZES[2].size,
        },
      ],
    ]);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('brushDirty_') && this.brushDirty_) {
      this.onBrushChanged_();
    }
  }

  protected onBrushClick_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const newType = targetElement.dataset['brush'] as AnnotationBrushType;
    this.currentType_ = newType;
    this.brushDirty_ = true;
  }

  protected onSizeClick_(e: Event) {
    const targetElement = e.currentTarget as HTMLElement;
    const size = Number(targetElement.dataset['size']);

    const currentBrush = this.getCurrentBrush_();
    if (currentBrush.size === size) {
      return;
    }

    currentBrush.size = size;
    this.brushDirty_ = true;
  }

  protected getCurrentBrushSizes_(): SizeOption[] {
    switch (this.currentType_) {
      case AnnotationBrushType.ERASER:
        return ERASER_SIZES;
      case AnnotationBrushType.HIGHLIGHTER:
        return HIGHLIGHTER_SIZES;
      case AnnotationBrushType.PEN:
        return PEN_SIZES;
    }
  }

  /**
   * When the brush changes, the new brush should be sent to
   * `this.pluginController_`.
   */
  private onBrushChanged_(): void {
    this.pluginController_.setAnnotationBrush(this.getCurrentBrush_());
    this.brushDirty_ = false;
  }

  private getCurrentBrush_(): AnnotationBrush {
    const brush = this.brushes_.get(this.currentType_);
    assert(brush);
    return brush;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-side-panel': ViewerSidePanelElement;
  }
}

customElements.define(ViewerSidePanelElement.is, ViewerSidePanelElement);
