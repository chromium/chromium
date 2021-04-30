// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../app.js'; /* <read-later-app> */

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class SidePanel extends PolymerElement {
  static get is() {
    return 'side-panel';
  }

  static get template() {
    return html`
      <read-later-app></read-later-app>
    `;
  }
}
customElements.define(SidePanel.is, SidePanel);
