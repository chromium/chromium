// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class assists with processing repeated tree changes in nontrivial ways
 * by allowing only the most recent tree change to be processed.
 */
export class RepeatedTreeChangeHandler {
  private changeStack_: chrome.automation.TreeChange[] = [];
  private callback_: (change: chrome.automation.TreeChange) => void;
  private handler_: (change: chrome.automation.TreeChange) => void;

  /**
   * A predicate for which tree changes are of interest. If none is provided,
   * default to always return true.
   */
  private predicate_: (change: chrome.automation.TreeChange) => boolean;

  /**
   * @param options |predicate| A generic predicate that filters for
   *     changes of interest.
   */
  constructor(
      filter: chrome.automation.TreeChangeObserverFilter,
      callback: (change: chrome.automation.TreeChange) => void, options: {
        predicate?: (change: chrome.automation.TreeChange) => boolean,
      } = {}) {
    this.callback_ = callback;
    this.predicate_ = options.predicate || (() => true);
    this.handler_ = change => this.onChange_(change);

    chrome.automation.addTreeChangeObserver(filter, this.handler_);
  }

  private onChange_(change: chrome.automation.TreeChange): void {
    if (this.predicate_(change)) {
      this.changeStack_.push(change);
      setTimeout(() => this.handleChange_(), 0);
    }
  }

  private handleChange_(): void {
    if (this.changeStack_.length === 0) {
      return;
    }

    const change = this.changeStack_.pop();
    this.changeStack_ = [];

    // TODO(b/314203187): Not null asserted, check these to make sure this is
    // correct.
    this.callback_(change!);
  }
}
