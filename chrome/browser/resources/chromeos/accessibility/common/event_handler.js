// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;

/**
 * This class wraps AutomationNode event listeners, adding some convenience
 * functions.
 */
export class EventHandler {
  /**
   * @param {!AutomationNode | !Array<!AutomationNode>} nodes
   * @param {!EventType | !Array<!EventType>} types
   * @param {?function(!AutomationEvent)} callback
   * @param {{capture: (boolean|undefined), exactMatch: (boolean|undefined),
   *     listenOnce: (boolean|undefined), predicate:
   *     ((function(AutomationEvent): boolean)|undefined)}}
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
    /** @private {!Array<!AutomationNode>} */
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];

    /** @private {!Array<!EventType>} */
    this.types_ = types instanceof Array ? types : [types];

    /** @private {?function(!AutomationEvent)} */
    this.callback_ = callback;

    /** @private {boolean} */
    this.capture_ = options.capture || false;

    /** @private {boolean} */
    this.exactMatch_ = options.exactMatch || false;

    /** @private {boolean} */
    this.listenOnce_ = options.listenOnce || false;

    /**
     * Default is a function that always returns true.
     * @private {!function(AutomationEvent): boolean}
     */
    this.predicate_ = options.predicate || (e => true);

    /** @private {boolean} */
    this.listening_ = false;

    /** @private {!function(AutomationEvent)} */
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

  /**
   * @return {boolean} Whether this EventHandler is currently listening for
   *     events.
   */
  listening() {
    return this.listening_;
  }

  /** @param {?function(!AutomationEvent)} callback */
  setCallback(callback) {
    this.callback_ = callback;
  }

  /**
   * Changes what nodes are being listened to. Removes listeners from existing
   *     nodes before adding listeners on new nodes.
   * @param {!AutomationNode | !Array<!AutomationNode>} nodes
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
   * @param {!AutomationNode} node
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
   * @param {!AutomationNode} node
   */
  removeNode(node) {
    this.nodes_ = this.nodes_.filter(n => n !== node);

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
