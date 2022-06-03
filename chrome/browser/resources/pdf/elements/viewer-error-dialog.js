// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerErrorDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-error-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {?function(): undefined} */
      reloadFn: Function,
    };
  }

  /** @private */
  onReload_() {
    if (this.reloadFn) {
      this.reloadFn();
    }
  }
}

customElements.define(ViewerErrorDialogElement.is, ViewerErrorDialogElement);
