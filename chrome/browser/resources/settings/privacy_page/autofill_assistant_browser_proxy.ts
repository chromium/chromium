// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview Handles the consent dialog and creating the audit record for
 * Autofill Assistant functionality on Desktop.
 */

export interface AutofillAssistantBrowserProxy {
  /**
   * Show the consent prompt for Autofill Assistant.
   * @return A promise firing with a boolean whether the consent was accepted.
   */
  promptForConsent(): Promise<boolean>;

  /**
   * Create an audit record that Autofill Assistant consent has been revoked.
   * @param dialogElements The strings contained in the clicked UI element.
   */
  revokeConsent(dialogElements: string[]): void;
}

export class AutofillAssistantBrowserProxyImpl implements
    AutofillAssistantBrowserProxy {
  promptForConsent() {
    return sendWithPromise('PromptForAutofillAssistantConsent');
  }

  revokeConsent(dialogElements: string[]) {
    chrome.send('RevokeAutofillAssistantConsent', dialogElements);
  }

  static getInstance(): AutofillAssistantBrowserProxy {
    return instance || (instance = new AutofillAssistantBrowserProxyImpl());
  }

  static setInstance(obj: AutofillAssistantBrowserProxy) {
    instance = obj;
  }
}

let instance: AutofillAssistantBrowserProxy|null = null;