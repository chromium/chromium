// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_template.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cros_components/button/button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionApprovalsBefore} from './flows/extension_approvals_before.js';
import {isParentAccessJellyEnabled, ParentAccessEvent} from './parent_access_app.js';
import {getTemplate} from './parent_access_before.html.js';
import {ParentAccessScreen} from './parent_access_screen.js';
import {ParentAccessParams_FlowType} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams} from './parent_access_ui_handler.js';

class ParentAccessBefore extends PolymerElement implements ParentAccessScreen {
  static get is() {
    return 'parent-access-before';
  }

  static get template() {
    return getTemplate();
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
      case ParentAccessParams_FlowType.kExtensionAccess:
        this.shadowRoot!.querySelector('#before-screen-body')!.appendChild(
            new ExtensionApprovalsBefore());
        return;
      default:
        return;
    }
  }

  private showParentAccessUi() {
    this.dispatchEvent(
        new CustomEvent(ParentAccessEvent.SHOW_AUTHENTICATION_FLOW, {
          bubbles: true,
          composed: true,
        }));
  }
}

customElements.define(ParentAccessBefore.is, ParentAccessBefore);
