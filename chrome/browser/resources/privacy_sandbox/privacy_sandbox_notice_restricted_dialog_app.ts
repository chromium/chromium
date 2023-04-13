// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogResizeMixin} from './privacy_sandbox_dialog_resize_mixin.js';
import {getTemplate} from './privacy_sandbox_notice_restricted_dialog_app.html.js';

const PrivacySandboxNoticeRestrictedDialogAppElementBase =
    PrivacySandboxDialogResizeMixin(PolymerElement);

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

    // TODO(b/277180532): fire the appropriate events, add more structure, etc.
    this.resizeAndShowNativeDialog();
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
