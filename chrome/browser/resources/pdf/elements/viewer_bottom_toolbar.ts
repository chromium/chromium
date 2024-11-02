// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_brush_selector.js';
import './ink_size_selector.js';
import './viewer_bottom_toolbar_dropdown.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

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
      currentSize: {type: Number},
      currentType: {type: String},
    };
  }

  currentSize: number = 0;
  currentType: AnnotationBrushType = AnnotationBrushType.PEN;
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-bottom-toolbar': ViewerBottomToolbarElement;
  }
}

customElements.define(
    ViewerBottomToolbarElement.is, ViewerBottomToolbarElement);
