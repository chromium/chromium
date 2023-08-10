// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_template.js';
import './supervision/supervised_user_offline.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ParentAccessOffline extends PolymerElement {
  static get is() {
    return 'parent-access-offline';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(ParentAccessOffline.is, ParentAccessOffline);
