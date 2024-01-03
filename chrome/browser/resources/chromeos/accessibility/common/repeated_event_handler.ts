// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AutomationNode = chrome.automation.AutomationNode;
type AutomationEvent = chrome.automation.AutomationEvent;
import EventType = chrome.automation.EventType;

interface RepeatedEventHandlerOptions {
  /**
   * True if events should only be handled if the provided node is the target.
   */
  exactMatch?: boolean;

  /**
   * True if events for children of |node| should be handled before they reach
   * the target node; false to be handled after the target node.
   */
  capture?: boolean;

  /**
   * True if a listener should be added to all ancestors of the provided nodes.
   */
  allAncestors?: boolean;
}

/**
 * This class assists with processing repeated events in nontrivial ways by
 * allowing only the most recent event to be processed.
 */
export class RepeatedEventHandler {
  private eventStack_: AutomationEvent[] = [];
  private nodes_: AutomationNode[];
  private type_: EventType;
  private callback_: (event: AutomationEvent) => void;
  private exactMatch_: boolean;
  private capture_: boolean;
  private listening_ = false;
  private handler_: (event: AutomationEvent) => void;

  constructor(
      nodes: AutomationNode|AutomationNode[], type: EventType,
      callback: (event: AutomationEvent) => void,
      options: RepeatedEventHandlerOptions = {}) {
    this.nodes_ = nodes instanceof Array ? nodes : [nodes];

    if (options.allAncestors) {
      nodes = this.nodes_;  // Make sure nodes is an array.
      this.nodes_ = [];
      for (let node of nodes) {
        while (node) {
          this.nodes_.push(node);
          // TODO(b/314203187): Not null asserted, check these to make sure they
          // are correct.
          node = node.parent!;
        }
      }
    }

    this.type_ = type;
    this.callback_ = callback;
    this.exactMatch_ = options.exactMatch || false;
    this.capture_ = options.capture || false;
    this.handler_ = event => this.onEvent_(event);

    this.start();
  }

  /** Starts listening or handling events. */
  start(): void {
    if (this.listening_) {
      return;
    }
    this.listening_ = true;
    for (const node of this.nodes_) {
      node.addEventListener(this.type_, this.handler_, this.capture_);
    }
  }

  /** Stops listening or handling future events. */
  stop(): void {
    if (!this.listening_) {
      return;
    }
    this.listening_ = false;
    for (const node of this.nodes_) {
      node.removeEventListener(this.type_, this.handler_, this.capture_);
    }
  }

  private onEvent_(event: AutomationEvent): void {
    this.eventStack_.push(event);
    setTimeout(() => this.handleEvent_(), 0);
  }

  private handleEvent_(): void {
    if (!this.listening_ || this.eventStack_.length === 0) {
      return;
    }

    // TODO(b/314203187): Not null asserted, check these to make sure they are
    // correct.
    const event = this.eventStack_.pop()!;
    if (this.exactMatch_ && !this.nodes_.includes(event!.target)) {
      return;
    }
    this.eventStack_ = [];

    this.callback_(event);
  }
}
