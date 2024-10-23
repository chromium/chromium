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
      currentColor: {type: Object},
      currentSize: {type: Number},
      currentType: {type: String},
    };
  }

  // TODO(crbug.com/373672165): Set these to null once PdfViewerElement fully
  // handles these properties. For now, set them to actual default values to
  // pass PDFExtensionJSInk2Test.Ink2SidePanel.
  currentColor: Color|undefined = hexToColor(PEN_COLORS[0]!.color);
  currentSize: number = PEN_SIZES[2]!.size;
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;

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
    this.onBrushChanged_();
  }

  protected onBrushChange_(e: CustomEvent<{value: AnnotationBrushType}>) {
    this.currentType = e.detail.value;
    this.currentSize = this.getCurrentBrush_().size;
    this.currentColor = this.getCurrentBrush_().color;
  }

  protected onSizeChange_(e: CustomEvent<{value: number}>) {
    const size = e.detail.value;
    this.currentSize = size;
    this.getCurrentBrush_().size = size;
  }

  protected onColorChange_(e: CustomEvent<{value: Color}>) {
    assert(this.shouldShowColorOptions_());
    const color = e.detail.value;
    this.currentColor = color;
    this.getCurrentBrush_().color = color;
  }

  protected shouldShowColorOptions_(): boolean {
    return this.currentType !== AnnotationBrushType.ERASER;
  }

  /**
   * When the brush changes, the new brush should be sent to
   * `this.pluginController_`.
   */
  private onBrushChanged_(): void {
    this.pluginController_.setAnnotationBrush(this.getCurrentBrush_());
  }

  private getCurrentBrush_(): AnnotationBrush {
    const brush = this.brushes_.get(this.currentType);
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
