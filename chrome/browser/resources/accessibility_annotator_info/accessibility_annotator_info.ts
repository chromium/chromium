// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './accessibility_annotator_info.css.js';
import {getHtml} from './accessibility_annotator_info.html.js';

export class AccessibilityAnnotatorInfoElement extends CrLitElement {
  static get is() {
    return 'accessibility-annotator-info';
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
    'accessibility-annotator-info': AccessibilityAnnotatorInfoElement;
  }
}

customElements.define(
    AccessibilityAnnotatorInfoElement.is, AccessibilityAnnotatorInfoElement);
