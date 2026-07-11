// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';
import {browserProxyFactory} from './comments.mojom-webui.js';

export class CommentsAppElement extends CrLitElement {
  private commentsApi_ = browserProxyFactory.getInstance();

  static get is() {
    return 'comments-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.commentsApi_.handler.showUI();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comments-app': CommentsAppElement;
  }
}

customElements.define(CommentsAppElement.is, CommentsAppElement);
