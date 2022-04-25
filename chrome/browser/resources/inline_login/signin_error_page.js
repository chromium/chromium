// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'signin-error-page' handles signinError view from
 * `chrome/browser/resources/inline_login/inline_login_app.js`
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './account_manager_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class SigninErrorPageElement extends PolymerElement {
  static get is() {
    return 'signin-error-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(SigninErrorPageElement.is, SigninErrorPageElement);
