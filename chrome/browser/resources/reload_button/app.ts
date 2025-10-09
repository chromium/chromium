// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/444358999): implement the reload button

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './app.html.js';

export class ReloadButtonAppElement extends CrLitElement {
  static get is() {
    return 'reload-button-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
