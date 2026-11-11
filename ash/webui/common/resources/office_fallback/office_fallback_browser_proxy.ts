// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export abstract class OfficeFallbackBrowserProxy {
  static getInstance(): OfficeFallbackBrowserProxy {
    return instance || (instance = new OfficeFallbackBrowserProxyImpl());
  }

  static setInstance(obj: OfficeFallbackBrowserProxy) {
    instance = obj;
  }

  abstract getDialogArguments(): string;
}

class OfficeFallbackBrowserProxyImpl extends OfficeFallbackBrowserProxy {
  // JSON-encoded dialog arguments.
  getDialogArguments(): string {
    return chrome.getVariableValue('dialogArguments');
  }
}

let instance: OfficeFallbackBrowserProxy|null = null;
