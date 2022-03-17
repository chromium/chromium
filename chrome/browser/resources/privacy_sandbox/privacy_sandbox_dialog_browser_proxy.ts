// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

// All actions related to showing & interacting with the privacy sandbox
// dialog. Includes actions which do not impact consent / notice state.
// Must be kept in sync with the enum of the same name in
// privacy_sandbox_service.h.
export enum PrivacySandboxDialogAction {
  NOTICE_SHOWN = 0,
  NOTICE_OPEN_SETTINGS = 1,
  NOTICE_ACKNOWLEDGE = 2,
  NOTICE_DISMISS = 3,
  NOTICE_CLOSED_NO_INTERACTION = 4,
  CONSENT_SHOWN = 5,
  CONSENT_ACCEPTED = 6,
  CONSENT_DECLINED = 7,
  CONSENT_MORE_INFO_OPENED = 8,
  CONSENT_MORE_INFO_CLOSED = 9,
  CONSENT_CLOSED_NO_DECISION = 10,
}

export class PrivacySandboxDialogBrowserProxy {
  dialogActionOccurred(action: PrivacySandboxDialogAction) {
    chrome.send('dialogActionOccurred', [action]);
  }

  resizeDialog(height: number) {
    return sendWithPromise('resizeDialog', height);
  }

  showDialog() {
    chrome.send('showDialog');
  }

  static getInstance(): PrivacySandboxDialogBrowserProxy {
    return instance || (instance = new PrivacySandboxDialogBrowserProxy());
  }

  static setInstance(obj: PrivacySandboxDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacySandboxDialogBrowserProxy|null = null;
