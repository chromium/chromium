// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './mini_page.html.js';

/** Element used to display a stylized NTP image. */
class MiniPageElement extends PolymerElement {
  static get is() {
    return 'ntp-mini-page';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(MiniPageElement.is, MiniPageElement);
