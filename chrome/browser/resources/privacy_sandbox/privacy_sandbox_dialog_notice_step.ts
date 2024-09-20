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

import {PrivacySandboxDialogMixin} from './privacy_sandbox_dialog_mixin.js';
import {getTemplate} from './privacy_sandbox_dialog_notice_step.html.js';

const PrivacySandboxDialogNoticeStepElementBase =
    PrivacySandboxDialogMixin(PolymerElement);

export class PrivacySandboxDialogNoticeStepElement extends
    PrivacySandboxDialogNoticeStepElementBase {
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
        observer: 'onNoticeLearnMoreExpandedChanged',
      },
    };
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
