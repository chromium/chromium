// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A toolbar that rests with its content horizontally centered or
 * pushes itself to the bottom of a flex parent when [pinned].
 */

import '//resources/cr_elements/cr_shared_vars.css.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sp_footer.css.js';
import {getHtml} from './sp_footer.html.js';

export class SpFooterElement extends CrLitElement {
  static get is() {
    return 'sp-footer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pinned: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  pinned: boolean = false;

  override firstUpdated() {
    this.setAttribute('role', 'toolbar');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-footer': SpFooterElement;
  }
}

customElements.define(SpFooterElement.is, SpFooterElement);
