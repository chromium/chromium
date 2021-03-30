// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerErrorScreenElement extends PolymerElement {
  static get is() {
    return 'viewer-error-screen';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      reloadFn: Function,
    };
  }

  show() {
    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  }

  reload() {
    if (this.reloadFn) {
      this.reloadFn();
    }
  }
}

customElements.define(ViewerErrorScreenElement.is, ViewerErrorScreenElement);
