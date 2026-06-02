// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';
export class ContextHubAppElement extends CrLitElement {
  static get is() {
    return 'context-hub-app';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
    };
  }

  protected accessor message_: string =
      'Hello! This page is under construction.';
}

declare global {
  interface HTMLElementTagNameMap {
    'context-hub-app': ContextHubAppElement;
  }
}

customElements.define(ContextHubAppElement.is, ContextHubAppElement);
