// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

export class CommentsApp extends CrLitElement {
  static get is() {
    return 'comments-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comments-app': CommentsApp;
  }
}

customElements.define(CommentsApp.is, CommentsApp);
