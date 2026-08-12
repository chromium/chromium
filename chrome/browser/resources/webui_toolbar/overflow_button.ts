// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './overflow_button.html.js';
import {getCss} from './toolbar_button.css.js';
import {HelpBubbleAnchorMixin} from './toolbar_button.js';

const OverflowButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class OverflowButtonElement extends OverflowButtonElementBase {
  static get is() {
    return 'overflow-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

customElements.define(OverflowButtonElement.is, OverflowButtonElement);

declare global {
  interface HTMLElementTagNameMap {
    'overflow-button': OverflowButtonElement;
  }
}
