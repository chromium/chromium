// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PerformanceBrowserProxy {
  openBatterySaverFeedbackDialog(): void;
  openHighEfficiencyFeedbackDialog(): void;
}

export class PerformanceBrowserProxyImpl implements PerformanceBrowserProxy {
  openBatterySaverFeedbackDialog() {
    chrome.send('openBatterySaverFeedbackDialog');
  }

  openHighEfficiencyFeedbackDialog() {
    chrome.send('openHighEfficiencyFeedbackDialog');
  }

  static getInstance(): PerformanceBrowserProxy {
    return instance || (instance = new PerformanceBrowserProxyImpl());
  }

  static setInstance(obj: PerformanceBrowserProxy) {
    instance = obj;
  }
}

let instance: PerformanceBrowserProxy|null = null;
