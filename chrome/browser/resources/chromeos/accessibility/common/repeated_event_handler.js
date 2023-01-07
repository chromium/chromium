// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class assists with processing repeated events in nontrivial ways by
 * allowing only the most recent event to be processed.
 */
export class RepeatedEventHandler {
  /**
   * @param {!chrome.automation.AutomationNode |
   *     !Array<!chrome.automation.AutomationNode>} nodes
   * @param {!chrome.automation.EventType} type
   * @param {!function(!chrome.automation.AutomationEvent)} callback
   * @param {{exactMatch: (boolean|undefined), capture: (boolean|undefined),
   *     allAncestors: (boolean|undefined)}} options
   *    exactMatch True if events should only be handled if the provided node is
   *        the target.
   *    capture True if events for children of |node| should be handled before
   *        they reach the target node; false to be handled after the target
   *        node.
   *    allAncestors True if a listener should be added to all ancestors of the
   *        provided nodes.
   */
  constructor(nodes, type, callback, options = {}) {
    /** @private {!Array<!chrome.automation.AutomationEvent>} */
    this.eventStack_ = [];

    /** @private {!Array<chrome.automation.AutomationNode>} */
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];

    if (options.allAncestors) {
      nodes = this.nodes_;  // Make sure nodes is an array.
      this.nodes_ = [];
      for (let node of nodes) {
        while (node) {
          this.nodes_.push(node);
          node = node.parent;
        }
      }
    }

    /** @private {!chrome.automation.EventType} */
    this.type_ = type;

    /** @private {!function(!chrome.automation.AutomationEvent)} */
    this.callback_ = callback;

    /** @private {boolean} */
    this.exactMatch_ = options.exactMatch || false;

    /** @private {boolean} */
    this.capture_ = options.capture || false;

    /** @private {boolean} */
    this.listening_ = false;

    /** @private {!function(!chrome.automation.AutomationEvent)} */
    this.handler_ = event => this.onEvent_(event);

    this.start();
  }

  /** Starts listening or handling events. */
  start() {
    if (this.listening_) {
      return;
    }
    this.listening_ = true;
    for (const node of this.nodes_) {
      node.addEventListener(this.type_, this.handler_, this.capture_);
    }
  }

  /** Stops listening or handling future events. */
  stop() {
    if (!this.listening_) {
      return;
    }
    this.listening_ = false;
    for (const node of this.nodes_) {
      node.removeEventListener(this.type_, this.handler_, this.capture_);
    }
  }

  /**
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onEvent_(event) {
    this.eventStack_.push(event);
    setTimeout(() => this.handleEvent_( ), 0);
  }

  /** @private */
  handleEvent_() {
    if (!this.listening_ || this.eventStack_.length === 0) {
      return;
    }

    const event = this.eventStack_.pop();
    if (this.exactMatch_ && !this.nodes_.includes(event.target)) {
      return;
    }
    this.eventStack_ = [];

    this.callback_(event);
  }
}
