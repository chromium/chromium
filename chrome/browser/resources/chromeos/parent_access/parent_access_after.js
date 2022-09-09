// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './flows/local_web_approvals_after.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class ParentAccessAfter extends PolymerElement {
  static get is() {
    return 'parent-access-after';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    // TODO(b/199753153): Implement handlers for deny and approve buttons.
  }

  /**
   * @private
   * @return {boolean}
   */
  isLocalWebApprovalsFlow_() {
    // TODO(b/199753545): Use the passed in loadTimeData value for the flowtype
    // when it is available.
    return true;
  }
}
customElements.define(ParentAccessAfter.is, ParentAccessAfter);