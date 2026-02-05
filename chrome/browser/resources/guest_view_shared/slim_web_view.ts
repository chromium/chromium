// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './slim_web_view.css.js';
import {getHtml} from './slim_web_view.html.js';

export interface SlimWebViewElement {
  $: {
    input: HTMLElement,
  };
}

export class SlimWebViewElement extends CrLitElement {
  static get is() {
    // TODO(crbug.com/460804848): Rename to webview, which is a restricted name.
    return 'slim-webview';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      src: {type: String, reflect: true},
    };
  }

  accessor src: string = '';

  contentWindow: WindowProxy|null = null;

  private viewInstanceId: number;

  constructor() {
    super();

    this.viewInstanceId = chrome.slimWebViewPrivate.getNextId();
    chrome.slimWebViewPrivate.registerView(this.viewInstanceId, this);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'slim-webview': SlimWebViewElement;
  }
}

customElements.define(SlimWebViewElement.is, SlimWebViewElement);
