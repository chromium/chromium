// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FavIconInfo, FrameInfo, GraphChangeStreamInterface, GraphChangeStreamReceiver, GraphDump, GraphDumpRemote, PageInfo, ProcessInfo, WorkerInfo} from './discards.mojom-webui.js';
import {getTemplate} from './graph_tab_template.html.js';

class DiscardsGraphChangeStreamImpl implements GraphChangeStreamInterface {
  private contentWindow_: Window;

  constructor(contentWindow: Window) {
    this.contentWindow_ = contentWindow;
  }

  private postMessage_(type: string, data: object|number|bigint) {
    this.contentWindow_.postMessage([type, data], '*');
  }

  frameCreated(frame: FrameInfo) {
    this.postMessage_('frameCreated', frame);
  }

  pageCreated(page: PageInfo) {
    this.postMessage_('pageCreated', page);
  }

  processCreated(process: ProcessInfo) {
    this.postMessage_('processCreated', process);
  }

  workerCreated(worker: WorkerInfo) {
    this.postMessage_('workerCreated', worker);
  }

  frameChanged(frame: FrameInfo) {
    this.postMessage_('frameChanged', frame);
  }

  pageChanged(page: PageInfo) {
    this.postMessage_('pageChanged', page);
  }

  processChanged(process: ProcessInfo) {
    this.postMessage_('processChanged', process);
  }

  workerChanged(worker: WorkerInfo) {
    this.postMessage_('workerChanged', worker);
  }

  favIconDataAvailable(iconInfo: FavIconInfo) {
    this.postMessage_('favIconDataAvailable', iconInfo);
  }

  nodeDeleted(nodeId: bigint) {
    this.postMessage_('nodeDeleted', nodeId);
  }
}

interface GraphTabElement {
  $: {
    webView: HTMLElement&{contentWindow: Window},
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
  private contentWindow_: Window|null = null;

  override connectedCallback() {
    this.graphDump_ = GraphDump.getRemote();
  }

  override disconnectedCallback() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    this.changeListener_ = null;
  }

  /** @param event A request from the WebView. */
  private onMessage_(event: MessageEvent) {
    const message = event.data;
    const type = message[0] as string;
    const data = message[1] as object | number | bigint;
    switch (type) {
      case 'requestNodeDescriptions':
        // Forward the request through the mojoms and bounce the reply back.
        this.graphDump_!.requestNodeDescriptions(data as bigint[])
            .then(
                (descriptions) => this.contentWindow_!.postMessage(
                    ['nodeDescriptions', descriptions.nodeDescriptionsJson],
                    '*'));
        break;
    }
  }

  private onWebViewReady_() {
    this.contentWindow_ = this.$.webView.contentWindow!;
    this.changeListener_ =
        new DiscardsGraphChangeStreamImpl(this.contentWindow_!);
    this.client_ = new GraphChangeStreamReceiver(this.changeListener_);
    // Subscribe for graph updates.
    this.graphDump_!.subscribeToChanges(
        this.client_.$.bindNewPipeAndPassRemote());

    window.addEventListener('message', this.onMessage_.bind(this));
  }
}

customElements.define(GraphTabElement.is, GraphTabElement);
