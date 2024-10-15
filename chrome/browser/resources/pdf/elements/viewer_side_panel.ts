// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';
import './ink_color_selector.js';
import './ink_size_selector.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';
import type {AnnotationBrush, Color} from '../constants.js';
import {PluginController} from '../controller.js';
import {hexToColor} from '../pdf_viewer_utils.js';

import {HIGHLIGHTER_COLORS, PEN_COLORS} from './ink_color_selector.js';
import {ERASER_SIZES, HIGHLIGHTER_SIZES, PEN_SIZES} from './ink_size_selector.js';
import {getCss} from './viewer_side_panel.css.js';
import {getHtml} from './viewer_side_panel.html.js';

export class ViewerSidePanelElement extends CrLitElement {
  static get is() {
    return 'viewer-side-panel';
  }

  static override get styles() {
    return getCss();
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

  protected currentType_: AnnotationBrushType = AnnotationBrushType.PEN;

  // Indicates the brush has changes and should be updated in
  // `this.pluginController_`.
  private brushDirty_: boolean = false;

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
          size: ERASER_SIZES[2]!.size,
        },
      ],
      [
        AnnotationBrushType.HIGHLIGHTER,
        {
          type: AnnotationBrushType.HIGHLIGHTER,
          color: hexToColor(HIGHLIGHTER_COLORS[0]!.color),
          size: HIGHLIGHTER_SIZES[2]!.size,
        },
      ],
      [
        AnnotationBrushType.PEN,
        {
          type: AnnotationBrushType.PEN,
          color: hexToColor(PEN_COLORS[0]!.color),
          size: PEN_SIZES[2]!.size,
        },
      ],
    ]);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('brushDirty_') &&
        (this.brushDirty_ ||
         changedPrivateProperties.get('brushDirty_') === undefined)) {
      this.onBrushChanged_();
    }
  }

  protected onBrushChange_(e: CustomEvent<{type: AnnotationBrushType}>) {
    this.currentType_ = e.detail.type;
    this.brushDirty_ = true;
  }

  protected onSizeChange_(e: CustomEvent<{value: number}>) {
    this.getCurrentBrush_().size = e.detail.value;
    this.brushDirty_ = true;
  }

  protected onColorChange_(e: CustomEvent<{value: Color}>) {
    assert(this.shouldShowColorOptions_());

    this.getCurrentBrush_().color = e.detail.value;
    this.brushDirty_ = true;
  }

  protected shouldShowColorOptions_(): boolean {
    return this.currentType_ !== AnnotationBrushType.ERASER;
  }

  protected getCurrentSize_(): number {
    return this.getCurrentBrush_().size;
  }

  protected getCurrentColor_(): Color {
    const color = this.getCurrentBrush_().color;
    assert(color);
    return color;
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
