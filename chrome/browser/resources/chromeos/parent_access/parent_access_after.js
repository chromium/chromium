// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './flows/local_web_approvals_after.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LocalWebApprovalsAfterElement} from './flows/local_web_approvals_after.js';
import {ParentAccessParams, ParentAccessParams_FlowType} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from './parent_access_ui_handler.js';

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
    this.renderFlowSpecificContent_();
    // TODO(b/199753153): Implement handlers for deny and approve buttons.
  }

  /**
   * Renders the correct after screen based on the ParentAccessParams flowtype.
   * @private
   */
  async renderFlowSpecificContent_() {
    const response = await getParentAccessParams();
    switch (response.params.flowType) {
      case ParentAccessParams_FlowType.kWebsiteAccess:
        this.shadowRoot.querySelector('#after-screen-body')
            .appendChild(new LocalWebApprovalsAfterElement());
        return;
      default:
        return;
    }
  }
}

customElements.define(ParentAccessAfter.is, ParentAccessAfter);
