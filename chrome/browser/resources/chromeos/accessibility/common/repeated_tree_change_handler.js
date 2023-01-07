// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class assists with processing repeated tree changes in nontrivial ways
 * by allowing only the most recent tree change to be processed.
 */
export class RepeatedTreeChangeHandler {
  /**
   * @param {!chrome.automation.TreeChangeObserverFilter} filter
   * @param {!function(!chrome.automation.TreeChange)} callback
   * @param {{predicate: ((function(!chrome.automation.TreeChange): boolean) |
   *     undefined)}} options predicate A generic predicate that filters for
   *     changes of interest.
   */
  constructor(filter, callback, options = {}) {
    /** @private {!Array<!chrome.automation.TreeChange>} */
    this.changeStack_ = [];

    /** @private {!function(!chrome.automation.TreeChange)} */
    this.callback_ = callback;

    /**
     * A predicate for which tree changes are of interest. If none is provided,
     * default to always return true.
     * @private {!function(!chrome.automation.TreeChange)}
     */
    this.predicate_ = options.predicate || (c => true);

    /** @private {!function(!chrome.automation.TreeChange)} */
    this.handler_ = change => this.onChange_(change);

    chrome.automation.addTreeChangeObserver(filter, this.handler_);
  }

  /**
   * @param {!chrome.automation.TreeChange} change
   * @private
   */
  onChange_(change) {
    if (this.predicate_(change)) {
      this.changeStack_.push(change);
      setTimeout(() => this.handleChange_(), 0);
    }
  }

  /** @private */
  handleChange_() {
    if (this.changeStack_.length === 0) {
      return;
    }

    const change = this.changeStack_.pop();
    this.changeStack_ = [];

    this.callback_(change);
  }
}
