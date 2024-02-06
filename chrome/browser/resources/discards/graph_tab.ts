// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {GraphChangeStreamInterface, GraphDumpRemote} from './discards.mojom-webui.js';
import {GraphChangeStreamReceiver, GraphDump} from './discards.mojom-webui.js';
import {Graph} from './graph.js';
import {getTemplate} from './graph_tab.html.js';

interface GraphTabElement {
  $: {
    toolTips: HTMLDivElement,
    graphBody: SVGElement,
  };
}

class GraphTabElement extends PolymerElement {
  static get is() {
    return 'graph-tab';
  }

  static get template() {
    return getTemplate();
  }

  private client_: GraphChangeStreamReceiver;

  /**
   * The Mojo graph data source.
   */
  private graphDump_: GraphDumpRemote|null = null;

  /**
   * The graph change listener.
   */
  private changeListener_: GraphChangeStreamInterface|null = null;

  /**
   * The WebView's content window object.
   */
  private graph_: Graph|null = null;

  private resizeObserver_: ResizeObserver|null = null;
  private debouncer_: Debouncer|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.graph_ = new Graph(this.$.graphBody, this.$.toolTips);
    this.graph_.initialize();
    // Set up a resize listener to track the graph on resize.
    this.resizeObserver_ = new ResizeObserver(() => {
      this.debouncer_ =
          Debouncer.debounce(this.debouncer_, timeOut.after(20), () => {
            if (this.graph_) {
              this.graph_.onResize();
            }
          });
    });
    this.resizeObserver_.observe(this.$.graphBody);
    this.graphDump_ = GraphDump.getRemote();
    this.client_ = new GraphChangeStreamReceiver(this.graph_);
    // Subscribe for graph updates.
    this.graphDump_!.subscribeToChanges(
        this.client_.$.bindNewPipeAndPassRemote());
  }

  override disconnectedCallback() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    this.changeListener_ = null;
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
    this.graph_ = null;
  }

  // Handle request for node descriptions from the Graph.
  private onRequestNodeDescriptions_(event: CustomEvent<bigint[]>) {
    // Forward the request through the mojoms and bounce the reply back.
    this.graphDump_!.requestNodeDescriptions(event.detail)
        .then(
            (descriptions) => this.graph_!.nodeDescriptions(
                descriptions.nodeDescriptionsJson));
  }
}

customElements.define(GraphTabElement.is, GraphTabElement);
