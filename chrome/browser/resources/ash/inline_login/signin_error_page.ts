// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'signin-error-page' handles signinError view from
 * `chrome/browser/resources/inline_login/inline_login_app.js`
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './account_manager_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './signin_error_page.html.js';

export class SigninErrorPageElement extends PolymerElement {
  static get is() {
    return 'signin-error-page';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(SigninErrorPageElement.is, SigninErrorPageElement);
