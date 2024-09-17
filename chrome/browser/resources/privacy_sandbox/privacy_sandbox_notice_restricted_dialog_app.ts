// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {PrivacySandboxDialogMixin} from './privacy_sandbox_dialog_mixin.js';
import {PrivacySandboxDialogResizeMixin} from './privacy_sandbox_dialog_resize_mixin.js';
import {getTemplate} from './privacy_sandbox_notice_restricted_dialog_app.html.js';

const PrivacySandboxNoticeRestrictedDialogAppElementBase =
    PrivacySandboxDialogMixin(PrivacySandboxDialogResizeMixin(PolymerElement));

export class PrivacySandboxNoticeRestrictedDialogAppElement extends
    PrivacySandboxNoticeRestrictedDialogAppElementBase {
  static get is() {
    return 'privacy-sandbox-notice-restricted-dialog-app';
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();

    this.resizeAndShowNativeDialog().then(() => {
      this.updateScrollableContents();
      this.maybeShowMoreButton().then(
          () => this.promptActionOccurred(
              PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN));
    });
  }

  private onRestrictedNoticeAcknowledge() {
    this.promptActionOccurred(
        PrivacySandboxPromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE);
  }

  private onRestrictedNoticeOpenSettings() {
    this.promptActionOccurred(
        PrivacySandboxPromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS);
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-notice-restricted-dialog-app':
        PrivacySandboxNoticeRestrictedDialogAppElement;
  }
}

customElements.define(
    PrivacySandboxNoticeRestrictedDialogAppElement.is,
    PrivacySandboxNoticeRestrictedDialogAppElement);
