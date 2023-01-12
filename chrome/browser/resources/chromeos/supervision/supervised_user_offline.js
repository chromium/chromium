// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class SupervisedUserOffline extends PolymerElement {
  static get is() {
    return 'supervised-user-offline';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(SupervisedUserOffline.is, SupervisedUserOffline);