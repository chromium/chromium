// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './viewer-error-dialog.html.js';

export class ViewerErrorDialogElement extends PolymerElement {
  static get is() {
    return 'viewer-error-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      reloadFn: Function,
    };
  }

  reloadFn: (() => void)|null;

  private onReload_() {
    if (this.reloadFn) {
      this.reloadFn();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-error-dialog': ViewerErrorDialogElement;
  }
}

customElements.define(ViewerErrorDialogElement.is, ViewerErrorDialogElement);
