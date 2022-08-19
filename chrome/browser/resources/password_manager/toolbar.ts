// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.css.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toolbar.html.js';

export class PasswordManagerToolbarElement extends PolymerElement {
  static get is() {
    return 'password-manager-toolbar';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    PasswordManagerToolbarElement.is, PasswordManagerToolbarElement);
