// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_card_frame.html.js';

/**
 * @fileoverview
 * 'diagnostics-card-frame' is a styling wrapper for each component's diagnostic
 * card.
 */

class DiagnosticsCardFrameElement extends PolymerElement {
  static get is(): string {
    return 'diagnostics-card-frame';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-card-frame': DiagnosticsCardFrameElement;
  }
}

customElements.define(
    DiagnosticsCardFrameElement.is, DiagnosticsCardFrameElement);
