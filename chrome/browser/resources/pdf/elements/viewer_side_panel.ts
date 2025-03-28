// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';
import './ink_color_selector.js';
import './ink_size_selector.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import {InkAnnotationBrushMixin} from './ink_annotation_brush_mixin.js';
import {getCss} from './viewer_side_panel.css.js';
import {getHtml} from './viewer_side_panel.html.js';

const ViewerSidePanelElementBase = InkAnnotationBrushMixin(CrLitElement);

export class ViewerSidePanelElement extends ViewerSidePanelElementBase {
  static get is() {
    return 'viewer-side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected shouldShowBrushOptions_(): boolean {
    return this.currentType !== AnnotationBrushType.ERASER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-side-panel': ViewerSidePanelElement;
  }
}

customElements.define(ViewerSidePanelElement.is, ViewerSidePanelElement);
