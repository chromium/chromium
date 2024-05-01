// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { PolymerElement } from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import { getTemplate } from './app.html.js';

export class FocusModeAppElement extends PolymerElement {
  static get is() {
    return 'focus-mode-app' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FocusModeAppElement.is]: FocusModeAppElement;
  }
}

customElements.define(FocusModeAppElement.is, FocusModeAppElement);
