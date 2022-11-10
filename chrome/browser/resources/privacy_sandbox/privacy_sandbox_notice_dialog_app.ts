// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_notice_dialog_app.html.js';

export class PrivacySandboxNoticeDialogAppElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-notice-dialog-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-notice-dialog-app': PrivacySandboxNoticeDialogAppElement;
  }
}

customElements.define(
    PrivacySandboxNoticeDialogAppElement.is,
    PrivacySandboxNoticeDialogAppElement);
