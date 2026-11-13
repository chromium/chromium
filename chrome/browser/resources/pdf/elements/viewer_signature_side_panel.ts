// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_color_selector.js';
import './signature_selector.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {InkAnnotationSignatureMixin} from './ink_annotation_signature_mixin.js';
import {getCss} from './viewer_signature_side_panel.css.js';
import {getHtml} from './viewer_signature_side_panel.html.js';

const ViewerSignatureSidePanelElementBase =
    InkAnnotationSignatureMixin(CrLitElement);

export class ViewerSignatureSidePanelElement extends
    ViewerSignatureSidePanelElementBase {
  static get is() {
    return 'viewer-signature-side-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-signature-side-panel': ViewerSignatureSidePanelElement;
  }
}

customElements.define(
    ViewerSignatureSidePanelElement.is, ViewerSignatureSidePanelElement);
