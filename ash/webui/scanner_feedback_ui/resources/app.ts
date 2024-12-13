// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class ScannerFeedbackAppElement extends PolymerElement {
  static get is() {
    return 'scanner-feedback-app' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScannerFeedbackAppElement.is]: ScannerFeedbackAppElement;
  }
}

customElements.define(ScannerFeedbackAppElement.is, ScannerFeedbackAppElement);
