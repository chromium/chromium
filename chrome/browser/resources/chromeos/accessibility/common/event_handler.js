// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class wraps AutomationNode event listeners, adding some convenience
 * functions.
 */
class EventHandler {
  /**
   * @param {!chrome.automation.AutomationNode |
   *         !Array<!chrome.automation.AutomationNode>} nodes
   * @param {!chrome.automation.EventType |
   *     !Array<!chrome.automation.EventType>} types
   * @param {?function(!chrome.automation.AutomationEvent)} callback
   * @param {{capture: (boolean|undefined), exactMatch: (boolean|undefined),
   *     listenOnce: (boolean|undefined), predicate:
   *     ((function(chrome.automation.AutomationEvent): boolean)|undefined)}}
   *     options
   *   exactMatch Whether to ignore events where the target is not the provided
   *       node.
   *   capture True if the event should be processed before it has reached the
   *       target node, false if it should be processed after.
   *   listenOnce True if the event listeners should automatically be removed
   *       when the callback is called once.
   *   predicate A predicate for what events will be processed.
   */
  constructor(nodes, types, callback, options = {}) {
    /** @private {!Array<!chrome.automation.AutomationNode>} */
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];

    /** @private {!Array<!chrome.automation.EventType>} */
    this.types_ = types instanceof Array ? types : [types];

    /** @private {?function(!chrome.automation.AutomationEvent)} */
    this.callback_ = callback;

    /** @private {boolean} */
    this.capture_ = options.capture || false;

    /** @private {boolean} */
    this.exactMatch_ = options.exactMatch || false;

    /** @private {boolean} */
    this.listenOnce_ = options.listenOnce || false;

    /**
     * Default is a function that always returns true.
     * @private {!function(chrome.automation.AutomationEvent): boolean}
     */
    this.predicate_ = options.predicate || ((e) => true);

    /** @private {boolean} */
    this.listening_ = false;

    /** @private {!function(chrome.automation.AutomationEvent)} */
    this.handler_ = event => this.handleEvent_(event);
  }

  /** Starts listening to events. */
  start() {
    if (this.listening_) {
      return;
    }

    for (const node of this.nodes_) {
      for (const type of this.types_) {
        node.addEventListener(type, this.handler_, this.capture_);
      }
    }
    this.listening_ = true;
  }

  /** Stops listening or handling future events. */
  stop() {
    for (const node of this.nodes_) {
      for (const type of this.types_) {
        node.removeEventListener(type, this.handler_, this.capture_);
      }
    }
    this.listening_ = false;
  }

  /** @param {?function(!chrome.automation.AutomationEvent)} callback */
  setCallback(callback) {
    this.callback_ = callback;
  }

  /**
   * Changes what nodes are being listened to. Removes listeners from existing
   *     nodes before adding listeners on new nodes.
   * @param {!chrome.automation.AutomationNode |
   *     !Array<!chrome.automation.AutomationNode>} nodes
   */
  setNodes(nodes) {
    const wasListening = this.listening_;
    this.stop();
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];
    if (wasListening) {
      this.start();
    }
  }

  /**
   * Adds another node to the set of nodes being listened to.
   * @param {!chrome.automation.AutomationNode} node
   */
  addNode(node) {
    this.nodes_.push(node);

    if (this.listening_) {
      for (const type of this.types_) {
        node.addEventListener(type, this.handler_, this.capture_);
      }
    }
  }

  /**
   * Removes a specific node from the set of nodes being listened to.
   * @param {!chrome.automation.AutomationNode} node
   */
  removeNode(node) {
    this.nodes_ = this.nodes_.filter((n) => n !== node);

    if (this.listening_) {
      for (const type of this.types_) {
        node.removeEventListener(type, this.handler_, this.capture_);
      }
    }
  }

  /** @private */
  handleEvent_(event) {
    if (this.exactMatch_ && !this.nodes_.includes(event.target)) {
      return;
    }

    if (!this.predicate_(event)) {
      return;
    }

    if (this.listenOnce_) {
      this.stop();
    }

    if (this.callback_) {
      this.callback_(event);
    }
  }
}
