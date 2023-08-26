// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './flows/local_web_approvals_after.js';
import './parent_access_template.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cros_components/button/button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionApprovalsAfter} from './flows/extension_approvals_after.js';
import {LocalWebApprovalsAfterElement} from './flows/local_web_approvals_after.js';
import {isParentAccessJellyEnabled, ParentAccessEvent} from './parent_access_app.js';
import {ParentAccessScreenInterface} from './parent_access_screen.js';
import {ParentAccessParams_FlowType, ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUIHandler} from './parent_access_ui_handler.js';

/** @implements {ParentAccessScreenInterface} */
class ParentAccessAfter extends PolymerElement {
  constructor() {
    super();
    this.parentAccessUIHandler = getParentAccessUIHandler();
  }

  static get is() {
    return 'parent-access-after';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  ready() {
    super.ready();
    this.renderFlowSpecificContent();
    this.addEventListener(ParentAccessEvent.ON_SCREEN_SWITCHED, () => {
      // TODO (b/297564545): Clean up Jelly flag logic after Jelly is enabled.
      if (isParentAccessJellyEnabled()) {
        this.shadowRoot.querySelector('#action-button-jelly').focus();
      } else {
        this.shadowRoot.querySelector('#action-button').focus();
      }
    });
  }

  /** @override */
  async renderFlowSpecificContent() {
    const response = await getParentAccessParams();
    switch (response.params.flowType) {
      case ParentAccessParams_FlowType.kWebsiteAccess:
        this.shadowRoot.querySelector('#after-screen-body')
            .appendChild(new LocalWebApprovalsAfterElement());
        return;
      case ParentAccessParams_FlowType.kExtensionAccess:
        this.shadowRoot.querySelector('#after-screen-body')
            .appendChild(new ExtensionApprovalsAfter());
        return;
      default:
        return;
    }
  }

  /**
   * @param {Event} e
   * @private
   */
  onParentApproved_(e) {
    this.parentAccessUIHandler.onParentAccessDone(ParentAccessResult.kApproved);
  }

  /**
   * @param {Event} e
   * @private
   */
  onParentDeclined_(e) {
    this.parentAccessUIHandler.onParentAccessDone(ParentAccessResult.kDeclined);
  }
}

customElements.define(ParentAccessAfter.is, ParentAccessAfter);
