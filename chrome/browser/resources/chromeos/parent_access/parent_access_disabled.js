// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionApprovalsDisabled} from './flows/extension_approvals_disabled.js';
import {ParentAccessEvent} from './parent_access_app.js';
import {ParentAccessScreenInterface} from './parent_access_screen.js';
import {ParentAccessParams_FlowType, ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUIHandler} from './parent_access_ui_handler.js';

/** @implements {ParentAccessScreenInterface} */
class ParentAccessDisabled extends PolymerElement {
  static get is() {
    return 'parent-access-disabled';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.renderFlowSpecificContent();
    this.addEventListener(ParentAccessEvent.ON_SCREEN_SWITCHED, () => {
      this.shadowRoot.querySelector('.action-button').focus();
    });
  }

  /** @override */
  async renderFlowSpecificContent() {
    const response = await getParentAccessParams();
    switch (response.params.flowType) {
      case ParentAccessParams_FlowType.kExtensionAccess:
        this.shadowRoot.querySelector('#disabled-screen-content')
            .appendChild(new ExtensionApprovalsDisabled());
        return;
      default:
        return;
    }
  }

  /** @private */
  onDisabledScreenClosed_() {
    getParentAccessUIHandler().onParentAccessDone(ParentAccessResult.kDisabled);
  }
}

customElements.define(ParentAccessDisabled.is, ParentAccessDisabled);
