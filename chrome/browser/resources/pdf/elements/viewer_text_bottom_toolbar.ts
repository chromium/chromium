// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AnnotationText} from '../constants.js';
import {TextAlignment} from '../constants.js';
import {colorToHex} from '../pdf_viewer_utils.js';

import {InkAnnotationTextMixin} from './ink_annotation_text_mixin.js';
import {InkTextObserverMixin} from './ink_text_observer_mixin.js';
import type {ViewerBottomToolbarDropdownElement} from './viewer_bottom_toolbar_dropdown.js';
import {getCss} from './viewer_text_bottom_toolbar.css.js';
import {getHtml} from './viewer_text_bottom_toolbar.html.js';

const ViewerTextBottomToolbarElementBase =
    InkAnnotationTextMixin(InkTextObserverMixin(CrLitElement));

export interface ViewerTextBottomToolbarElement {
  $: {
    alignment: ViewerBottomToolbarDropdownElement,
    color: ViewerBottomToolbarDropdownElement,
  };
}

export class ViewerTextBottomToolbarElement extends
    ViewerTextBottomToolbarElementBase {
  static get is() {
    return 'viewer-text-bottom-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentAlignment_: {type: String},
    };
  }

  protected accessor currentAlignment_: TextAlignment = TextAlignment.LEFT;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('currentColor') && this.currentColor) {
      this.style.setProperty(
          '--ink-brush-color', colorToHex(this.currentColor));
    }
  }

  protected getAlignmentIcon_(): string {
    switch (this.currentAlignment_) {
      case TextAlignment.LEFT:
        return 'pdf:text-align-left';
      case TextAlignment.CENTER:
        return 'pdf:text-align-center';
      case TextAlignment.RIGHT:
        return 'pdf:text-align-right';
      case TextAlignment.JUSTIFY:
        return 'pdf:text-align-justify';
      default:
        assertNotReached();
    }
  }

  override onTextChanged(text: AnnotationText) {
    super.onTextChanged(text);
    this.currentAlignment_ = text.alignment;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-text-bottom-toolbar': ViewerTextBottomToolbarElement;
  }
}

customElements.define(
    ViewerTextBottomToolbarElement.is, ViewerTextBottomToolbarElement);
