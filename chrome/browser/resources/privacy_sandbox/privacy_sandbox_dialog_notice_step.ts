// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_dialog_notice_step.html.js';

export class PrivacySandboxDialogNoticeStepElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-dialog-notice-step';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: {
        type: Boolean,
        observer: 'onLearnMoreExpandedChanged_',
      },
    };
  }

  private onLearnMoreExpandedChanged_() {
    // TODO(crbug.com/1378703): Report learn more actions.
  }

  private onNoticeOpenSettings_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  }

  private onNoticeAcknowledge_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  }

  private promptActionOccurred(action: PrivacySandboxPromptAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(action);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-dialog-notice-step': PrivacySandboxDialogNoticeStepElement;
  }
}

customElements.define(
    PrivacySandboxDialogNoticeStepElement.is,
    PrivacySandboxDialogNoticeStepElement);
