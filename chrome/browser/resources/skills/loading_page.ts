// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './loading_page.css.js';
import {getHtml} from './loading_page.html.js';

export class LoadingPageElement extends CrLitElement {
  static get is() {
    return 'loading-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialog: {type: Boolean},
      editor: {type: Boolean},
    };
  }

  accessor dialog: boolean = false;
  accessor editor: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    if (!this.dialog && window.location.pathname === '/editor') {
      this.editor = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'loading-page': LoadingPageElement;
  }
}

customElements.define(LoadingPageElement.is, LoadingPageElement);
