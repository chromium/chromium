// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {getCss as getCrHiddenStyleLitCss} from 'chrome://resources/cr_elements/cr_hidden_style_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './viewer_error_dialog.html.js';

export class ViewerErrorDialogElement extends CrLitElement {
  static get is() {
    return 'viewer-error-dialog';
  }

  static override get styles() {
    return getCrHiddenStyleLitCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      reloadFn: {type: Object},
    };
  }

  reloadFn?: (() => void)|null;

  protected onReload_() {
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
