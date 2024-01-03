// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;

interface EventHandlerOptions {
  /**
   * Whether to ignore events where the target is not the provided node.
   */
  exactMatch?: boolean;

  /**
   * True if the event should be processed before it has reached the target
   * node, false if it should be processed after.
   */
  capture?: boolean;

  /**
   * True if the event listeners should automatically be removed when the
   * callback is called once.
   */
  listenOnce?: boolean;

  /**
   * A predicate for what events will be processed.
   */
  predicate?: (event: AutomationEvent) => boolean;
}

/**
 * This class wraps AutomationNode event listeners, adding some convenience
 * functions.
 */
export class EventHandler {
  private nodes_: AutomationNode[];
  private types_: EventType[];
  private callback_: ((event: AutomationEvent) => void)|null;
  private capture_: boolean;
  private exactMatch_: boolean;
  private listenOnce_: boolean;
  private listening_ = false;
  private predicate_: (event: AutomationEvent) => boolean;
  private handler_: (event: AutomationEvent) => void;

  constructor(
      nodes: AutomationNode|AutomationNode[], types: EventType|EventType[],
      callback: (event: AutomationEvent) => void,
      options: EventHandlerOptions = {}) {
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];
    this.types_ = types instanceof Array ? types : [types];
    this.callback_ = callback;
    this.capture_ = options.capture || false;
    this.exactMatch_ = options.exactMatch || false;
    this.listenOnce_ = options.listenOnce || false;

    /**
     * Default is a function that always returns true.
     */
    this.predicate_ = options.predicate || (_e => true);

    this.handler_ = event => this.handleEvent_(event);
  }

  /** Starts listening to events. */
  start(): void {
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
  stop(): void {
    for (const node of this.nodes_) {
      for (const type of this.types_) {
        node.removeEventListener(type, this.handler_, this.capture_);
      }
    }
    this.listening_ = false;
  }

  /**
   * @return Whether this EventHandler is currently listening for events.
   */
  listening(): boolean {
    return this.listening_;
  }

  setCallback(callback: ((event: AutomationEvent) => void)|null): void {
    this.callback_ = callback;
  }

  /**
   * Changes what nodes are being listened to. Removes listeners from existing
   *     nodes before adding listeners on new nodes.
   */
  setNodes(nodes: AutomationNode|AutomationNode[]): void {
    const wasListening = this.listening_;
    // TODO(b/318557827): Shouldn't this be: if (wasListening) this.stop()?
    this.stop();
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];
    if (wasListening) {
      this.start();
    }
  }

  /**
   * Adds another node to the set of nodes being listened to.
   */
  addNode(node: AutomationNode): void {
    this.nodes_.push(node);

    if (this.listening_) {
      for (const type of this.types_) {
        node.addEventListener(type, this.handler_, this.capture_);
      }
    }
  }

  /**
   * Removes a specific node from the set of nodes being listened to.
   */
  removeNode(node: AutomationNode): void {
    this.nodes_ = this.nodes_.filter(n => n !== node);

    if (this.listening_) {
      for (const type of this.types_) {
        node.removeEventListener(type, this.handler_, this.capture_);
      }
    }
  }

  private handleEvent_(event: AutomationEvent): void {
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
