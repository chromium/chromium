// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './strings.m.js';
import './shared_style.css.js';
import './privacy_sandbox_dialog_learn_more.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {PrivacySandboxDialogMixin} from './privacy_sandbox_dialog_mixin.js';
import {PrivacySandboxDialogResizeMixin} from './privacy_sandbox_dialog_resize_mixin.js';
import {getTemplate} from './privacy_sandbox_notice_dialog_app.html.js';

const PrivacySandboxNoticeDialogAppElementBase =
    PrivacySandboxDialogMixin(PrivacySandboxDialogResizeMixin(PolymerElement));

export class PrivacySandboxNoticeDialogAppElement extends
    PrivacySandboxNoticeDialogAppElementBase {
  static get is() {
    return 'privacy-sandbox-notice-dialog-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: {
        type: Boolean,
        observer: 'onNoticeLearnMoreExpandedChanged',
      },
    };
  }

  override ready() {
    super.ready();

    this.resizeAndShowNativeDialog().then(() => {
      this.updateScrollableContents();
      this.maybeShowMoreButton().then(
          () => this.promptActionOccurred(
              PrivacySandboxPromptAction.NOTICE_SHOWN));
    });
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
