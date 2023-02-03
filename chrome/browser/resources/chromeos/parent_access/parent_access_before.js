// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessScreenInterface} from './parent_access_screen.js';

/** @implements {ParentAccessScreenInterface} */
class ParentAccessBefore extends PolymerElement {
  constructor() {
    super();
  }

  static get is() {
    return 'parent-access-before';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.renderFlowSpecificContent();
  }

  /** @override */
  async renderFlowSpecificContent() {
    // TODO(b/262448127): Implement flow specific content.
  }

  /** @private */
  showParentAccessUI_() {
    this.dispatchEvent(new CustomEvent('show-authentication-flow', {
      bubbles: true,
      composed: true,
    }));
  }
}

customElements.define(ParentAccessBefore.is, ParentAccessBefore);
