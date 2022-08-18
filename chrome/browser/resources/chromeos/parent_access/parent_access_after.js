// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './flows/local_web_approvals_after.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'parent-access-after',

  _template: html`{__html_template__}`,

  /** @override */
  ready() {
    // TODO(b/199753153): Implement handlers for deny and approve buttons.
  },

  /**
   * @private
   * @return {boolean}
   */
  isLocalWebApprovalsFlow_() {
    // TODO(b/199753545): Use the passed in loadTimeData value for the flowtype
    // when it is available.
    return true;
  },
});