// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

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

  protected onManageSettingsClick_() {
    // Handle the manage settings click event here.
    // TODO(crbug.com/488321731): Connect this with handler via proxy.
  }

  protected onGotItClick_() {
    // Handle the got it click event here.
    // TODO(crbug.com/488321731): Connect this with handler via proxy.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accessibility-annotator-info': AccessibilityAnnotatorInfoElement;
  }
}

customElements.define(
    AccessibilityAnnotatorInfoElement.is, AccessibilityAnnotatorInfoElement);
