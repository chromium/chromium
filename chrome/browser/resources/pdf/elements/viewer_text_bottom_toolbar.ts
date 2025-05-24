// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes} from '../constants.js';
import {TextAlignment} from '../constants.js';
import {colorToHex} from '../pdf_viewer_utils.js';

import {InkAnnotationTextMixin} from './ink_annotation_text_mixin.js';
import {InkTextObserverMixin} from './ink_text_observer_mixin.js';
import type {ViewerBottomToolbarDropdownElement} from './viewer_bottom_toolbar_dropdown.js';
import {getCss} from './viewer_text_bottom_toolbar.css.js';
import {getHtml} from './viewer_text_bottom_toolbar.html.js';

const ViewerTextBottomToolbarElementBase =
    InkAnnotationTextMixin(InkTextObserverMixin(I18nMixinLit(CrLitElement)));

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
        return 'pdf-ink:text-align-left';
      case TextAlignment.CENTER:
        return 'pdf-ink:text-align-center';
      case TextAlignment.RIGHT:
        return 'pdf-ink:text-align-right';
      default:
        assertNotReached();
    }
  }

  override onTextAttributesChanged(attributes: TextAttributes) {
    super.onTextAttributesChanged(attributes);
    this.currentAlignment_ = attributes.alignment;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-text-bottom-toolbar': ViewerTextBottomToolbarElement;
  }
}

customElements.define(
    ViewerTextBottomToolbarElement.is, ViewerTextBottomToolbarElement);
