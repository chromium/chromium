// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A toolbar that rests with its content horizontally centered or
 * pushes itself to the bottom of a flex parent when [pinned].
 */

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_footer.html.js';

export class SpFooterElement extends PolymerElement {
  static get is() {
    return 'sp-footer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pinned: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  pinned: boolean;

  override ready() {
    super.ready();
    this.setAttribute('role', 'toolbar');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-footer': SpFooterElement;
  }
}

customElements.define(SpFooterElement.is, SpFooterElement);
