// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './signout_confirmation_app.html.js';

export class SignoutConfirmationAppElement extends CrLitElement {
  static get is() {
    return 'signout-confirmation-app';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signout-confirmation-app': SignoutConfirmationAppElement;
  }
}

customElements.define(
    SignoutConfirmationAppElement.is, SignoutConfirmationAppElement);
