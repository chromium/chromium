// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'signin-blocked-by-policy-page' handles signinBlockedByPolicy view from
 * `chrome/browser/resources/inline_login/inline_login_app.js`
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './account_manager_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './signin_blocked_by_policy_page.html.js';

export class SigninBlockedByPolicyPageElement extends PolymerElement {
  static get is() {
    return 'signin-blocked-by-policy-page';
  }

  static get template() {
    return getTemplate();
  }

  /**
   * Gets body text with the respective user email and hosted domain for the
   * user that went through the sign-in flow.
   * @param email User's email used in the sign-in flow.
   * @param hostedDomain Hosted domain of the user's email used in the sign-in
   *     flow.
   * @param deviceType name of the Chrome device type (e.g. Chromebook,
   *     Chromebox).
   */
  private getBodyText_(email: string, hostedDomain: string, deviceType: string):
      TrustedHTML {
    return sanitizeInnerHtml(loadTimeData.getStringF(
        'accountManagerDialogSigninBlockedByPolicyBody', email, hostedDomain,
        deviceType));
  }
}

customElements.define(
    SigninBlockedByPolicyPageElement.is, SigninBlockedByPolicyPageElement);
