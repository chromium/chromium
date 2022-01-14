// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * screensaver.
 */

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class AmbientPreview extends PolymerElement {
  static get is() {
    return 'ambient-preview';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }
}
customElements.define(AmbientPreview.is, AmbientPreview);
