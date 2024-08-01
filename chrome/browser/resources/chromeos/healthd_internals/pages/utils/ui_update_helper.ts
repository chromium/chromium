// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper class to maintain tasks for rendering UI in page.
 */
export class UiUpdateHelper {
  constructor(updateHandler: Function) {
    this.updateHandler = updateHandler;
  }

  // Init in constructor.
  private updateHandler: Function;

  // Whether the page is visible. If not, we don't update UI to reduce CPU
  // usage.
  private isVisible: boolean = false;

  // The update interval for UI in milliseconds.
  private updateInterval?: number = undefined;

  // The UI update interval ID used for cancelling the running interval.
  private updateIntervalId?: number = undefined;

  updateVisibility(isVisible: boolean) {
    this.isVisible = isVisible;
    this.setupUiUpdateRequests();
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateInterval = intervalSeconds * 1000;
    this.setupUiUpdateRequests();
  }

  private setupUiUpdateRequests() {
    this.cancelUiUpdateRequests();

    if (!this.isVisible || this.updateInterval === undefined ||
        this.updateInterval === 0) {
      return;
    }
    const update = () => this.updateHandler();
    this.updateIntervalId = setInterval(update, this.updateInterval);
    update();
  }

  private cancelUiUpdateRequests() {
    if (this.updateIntervalId === undefined) {
      return;
    }
    clearInterval(this.updateIntervalId);
    this.updateIntervalId = undefined;
  }
}
