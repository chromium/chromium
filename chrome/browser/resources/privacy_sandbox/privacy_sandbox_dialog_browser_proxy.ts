// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

// All actions related to showing & interacting with the privacy sandbox
// prompt. Includes actions which do not impact consent / notice state.
// Must be kept in sync with the enum of the same name in
// privacy_sandbox_service.h.
export enum PrivacySandboxPromptAction {
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
  NOTICE_LEARN_MORE = 11,
  NOTICE_MORE_INFO_OPENED = 12,
  NOTICE_MORE_INFO_CLOSED = 13,
  CONSENT_MORE_BUTTON_CLICKED = 14,
  NOTICE_MORE_BUTTON_CLICKED = 15,
  RESTRICTED_NOTICE_ACKNOWLEDGE = 16,
  RESTRICTED_NOTICE_OPEN_SETTINGS = 17,
  RESTRICTED_NOTICE_SHOWN = 18,
  RESTRICTED_NOTICE_CLOSED_NO_INTERACTION = 19,
  RESTRICTED_NOTICE_MORE_BUTTON_CLICKED = 20,
  PRIVACY_POLICY_LINK_CLICKED = 21,
}

export class PrivacySandboxDialogBrowserProxy {
  recordPrivacyPolicyLoadTime(privacyPolicyLoadDuration: number) {
    chrome.send('recordPrivacyPolicyLoadTime', [privacyPolicyLoadDuration]);
  }
  promptActionOccurred(action: PrivacySandboxPromptAction) {
    chrome.send('promptActionOccurred', [action]);
  }

  resizeDialog(height: number) {
    return sendWithPromise('resizeDialog', height);
  }

  showDialog() {
    chrome.send('showDialog');
  }

  shouldShowPrivacySandboxPrivacyPolicy() {
    return sendWithPromise('shouldShowPrivacySandboxPrivacyPolicy');
  }

  static getInstance(): PrivacySandboxDialogBrowserProxy {
    return instance || (instance = new PrivacySandboxDialogBrowserProxy());
  }

  static setInstance(obj: PrivacySandboxDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacySandboxDialogBrowserProxy|null = null;
