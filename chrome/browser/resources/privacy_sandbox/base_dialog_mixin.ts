// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {BaseDialogBrowserProxy} from './base_dialog_browser_proxy.js';
import type {PrivacySandboxNotice} from './notice.mojom-webui.js';
import {PrivacySandboxNoticeEvent} from './notice.mojom-webui.js';

type Constructor<T> = new (...args: any[]) => T;

export const BaseDialogMixin = <T extends Constructor<CrLitElement>>(
    superClass: T,
    notice: PrivacySandboxNotice): T&Constructor<BaseDialogMixinInterface> => {
  class BaseDialogMixin extends superClass implements BaseDialogMixinInterface {
    private handler_: BaseDialogPageHandlerInterface;
    private notice_: PrivacySandboxNotice;

    constructor(...args: any[]) {
      super(...args);
      this.notice_ = notice;
    }

    override firstUpdated() {
      this.handler_ = BaseDialogBrowserProxy.getInstance().handler;
    }

    onOptIn() {
      this.handler_.eventOccurred(
          this.notice_, PrivacySandboxNoticeEvent.kOptIn);
    }

    onAck() {
      this.handler_.eventOccurred(this.notice_, PrivacySandboxNoticeEvent.kAck);
    }
  }
  return BaseDialogMixin;
};

export interface BaseDialogMixinInterface {
  onOptIn(): void;
  onAck(): void;
}
