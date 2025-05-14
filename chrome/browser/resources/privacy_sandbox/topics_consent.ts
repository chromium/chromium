// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {BaseDialogBrowserProxy} from './base_dialog_browser_proxy.js';
import {PrivacySandboxNotice, PrivacySandboxNoticeEvent} from './notice.mojom-webui.js';
import {getHtml} from './topics_consent.html.js';

export class TopicsConsent extends CrLitElement {
  static get is() {
    return 'topics-consent';
  }

  override render() {
    return getHtml.bind(this)();
  }

  private handler_: BaseDialogPageHandlerInterface;

  override firstUpdated() {
    this.handler_ = BaseDialogBrowserProxy.getInstance().handler;
  }

  protected onConsentButton_() {
    this.handler_.eventOccurred(
        PrivacySandboxNotice.kTopicsConsentNotice,
        PrivacySandboxNoticeEvent.kOptIn);
    this.handler_.closeDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topics-consent': TopicsConsent;
  }
}

customElements.define(TopicsConsent.is, TopicsConsent);
