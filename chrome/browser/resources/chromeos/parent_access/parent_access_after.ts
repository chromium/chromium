// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './flows/local_web_approvals_after.js';
import './parent_access_template.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cros_components/button/button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionApprovalsAfter} from './flows/extension_approvals_after.js';
import {LocalWebApprovalsAfter} from './flows/local_web_approvals_after.js';
import {getTemplate} from './parent_access_after.html.js';
import {isParentAccessJellyEnabled, ParentAccessEvent} from './parent_access_app.js';
import {ParentAccessScreen} from './parent_access_screen.js';
import {ParentAccessParams_FlowType, ParentAccessResult, ParentAccessUiHandlerInterface} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUiHandler} from './parent_access_ui_handler.js';

class ParentAccessAfter extends PolymerElement implements ParentAccessScreen {
  static get is() {
    return 'parent-access-after';
  }

  static get template() {
    return getTemplate();
  }

  private parentAccessUiHandler: ParentAccessUiHandlerInterface;

  constructor() {
    super();
    this.parentAccessUiHandler = getParentAccessUiHandler();
  }

  override ready() {
    super.ready();
    this.renderFlowSpecificContent();
    this.addEventListener(ParentAccessEvent.ON_SCREEN_SWITCHED, () => {
      // TODO (b/297564545): Clean up Jelly flag logic after Jelly is enabled.
      if (isParentAccessJellyEnabled()) {
        this.shadowRoot!.querySelector<HTMLElement>(
                            '#action-button-jelly')!.focus();
      } else {
        this.shadowRoot!.querySelector<HTMLElement>('#action-button')!.focus();
      }
    });
  }

  async renderFlowSpecificContent() {
    const response = await getParentAccessParams();
    switch (response!.params.flowType) {
      case ParentAccessParams_FlowType.kWebsiteAccess:
        this.shadowRoot!.querySelector('#after-screen-body')!.appendChild(
            new LocalWebApprovalsAfter());
        return;
      case ParentAccessParams_FlowType.kExtensionAccess:
        this.shadowRoot!.querySelector('#after-screen-body')!.appendChild(
            new ExtensionApprovalsAfter());
        return;
      default:
        return;
    }
  }

  private onParentApproved() {
    this.parentAccessUiHandler.onParentAccessDone(ParentAccessResult.kApproved);
  }

  private onParentDeclined() {
    this.parentAccessUiHandler.onParentAccessDone(ParentAccessResult.kDeclined);
  }
}

customElements.define(ParentAccessAfter.is, ParentAccessAfter);
