// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class PrivacySandboxDialogBrowserProxy {
  closeDialog() {
    chrome.send('closeDialog');
  }

  static getInstance(): PrivacySandboxDialogBrowserProxy {
    return instance || (instance = new PrivacySandboxDialogBrowserProxy());
  }

  static setInstance(obj: PrivacySandboxDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacySandboxDialogBrowserProxy|null = null;
