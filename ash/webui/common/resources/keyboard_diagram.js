// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './keyboard_key.js'

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'keyboard-diagram' displays a diagram of a CrOS-style keyboard.
 */

export class KeyboardDiagramElement extends PolymerElement {
  static get is() {
    return 'keyboard-diagram';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(KeyboardDiagramElement.is, KeyboardDiagramElement);
