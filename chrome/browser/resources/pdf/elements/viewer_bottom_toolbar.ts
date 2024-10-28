// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import {getCss} from './viewer_bottom_toolbar.css.js';
import {getHtml} from './viewer_bottom_toolbar.html.js';

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
      currentType: {type: String},
    };
  }

  currentType: AnnotationBrushType = AnnotationBrushType.PEN;
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-bottom-toolbar': ViewerBottomToolbarElement;
  }
}

customElements.define(
    ViewerBottomToolbarElement.is, ViewerBottomToolbarElement);
