// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ExtensionsHatsBrowserProxy {
  triggerSurvey(): void;
  extensionKeptAction(): void;
  extensionRemovedAction(): void;
  nonTriggerExtensionRemovedAction(): void;
  removeAllAction(numberOfExtensionsRemoved: number): void;
}

export class ExtensionsHatsBrowserProxyImpl implements
    ExtensionsHatsBrowserProxy {
  triggerSurvey() {
    chrome.send('extensionsSafetyHubTriggerSurvey');
  }

  extensionKeptAction() {
    chrome.send('extensionsSafetyHubExtensionKept');
  }

  extensionRemovedAction() {
    chrome.send('extensionsSafetyHubExtensionRemoved');
  }

  nonTriggerExtensionRemovedAction() {
    chrome.send('extensionsSafetyHubNonTriggerExtensionRemoved');
  }

  removeAllAction(numberOfExtensionsRemoved: number) {
    chrome.send('extensionsSafetyHubRemoveAll', [numberOfExtensionsRemoved]);
  }

  static getInstance(): ExtensionsHatsBrowserProxy {
    return instance || (instance = new ExtensionsHatsBrowserProxyImpl());
  }

  static setInstance(obj: ExtensionsHatsBrowserProxy) {
    instance = obj;
  }
}

let instance: ExtensionsHatsBrowserProxy|null = null;
