// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './empty_section.css.js';
import {getHtml} from './empty_section.html.js';

export class EmptySectionElement extends CrLitElement {
  static get is() {
    return 'empty-section';
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
    'empty-section': EmptySectionElement;
  }
}

customElements.define(EmptySectionElement.is, EmptySectionElement);
