// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {GraphDumpRemote} from './discards.mojom-webui.js';
import {GraphChangeStreamReceiver, GraphDump} from './discards.mojom-webui.js';
import {Graph} from './graph.js';
import {getCss} from './graph_tab.css.js';
import {getHtml} from './graph_tab.html.js';

export interface GraphTabElement {
  $: {
    toolTips: HTMLElement,
    graphBody: SVGElement,
  };
}

export class GraphTabElement extends CrLitElement {
  static get is() {
    return 'graph-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  /**
   * The Mojo graph data source.
   */
  private graphDump_: GraphDumpRemote|null = null;

  /**
   * The WebView's content window object.
   */
  private graph_: Graph|null = null;

  private resizeObserver_: ResizeObserver|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.graph_ = new Graph(this.$.graphBody, this.$.toolTips);
    this.graph_.initialize();
    // Set up a resize listener to track the graph on resize.
    this.resizeObserver_ = new ResizeObserver(() => {
      if (this.graph_) {
        this.graph_.onResize();
      }
    });
    this.resizeObserver_.observe(this.$.graphBody);
    this.graphDump_ = GraphDump.getRemote();
    const client = new GraphChangeStreamReceiver(this.graph_);
    // Subscribe for graph updates.
    this.graphDump_.subscribeToChanges(client.$.bindNewPipeAndPassRemote());
  }

  override disconnectedCallback() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
    this.graph_ = null;
  }

  // Handle request for node descriptions from the Graph.
  protected onRequestNodeDescriptions_(event: CustomEvent<bigint[]>) {
    // Forward the request through the mojoms and bounce the reply back.
    this.graphDump_!.requestNodeDescriptions(event.detail)
        .then(
            (descriptions) => this.graph_!.nodeDescriptions(
                descriptions.nodeDescriptionsJson));
  }
}

customElements.define(GraphTabElement.is, GraphTabElement);
