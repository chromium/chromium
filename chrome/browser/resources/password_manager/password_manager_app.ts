// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_manager_app.html.js';

export class PasswordManagerAppElement extends PolymerElement {
  static get is() {
    return 'password-manager-app';
  }

  static get template() {
    return getTemplate();
  }
}
customElements.define(PasswordManagerAppElement.is, PasswordManagerAppElement);
