// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The user-subpage component displays information about the
 * current user and allows changing device avatar image.
 */

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class UserSubpage extends PolymerElement {
  static get is() {
    return 'user-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }
}

customElements.define(UserSubpage.is, UserSubpage);
