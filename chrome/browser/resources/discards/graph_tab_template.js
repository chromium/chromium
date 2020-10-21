// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './mojo_api.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @implements {discards.mojom.GraphChangeStreamInterface}
 */
class DiscardsGraphChangeStreamImpl {
  /** @param {Window} contentWindow */
  constructor(contentWindow) {
    /** @private {Window} */
    this.contentWindow_ = contentWindow;
  }

  /**
   * @param {string} type
   * @param {Object|number} data
   */
  postMessage_(type, data) {
    this.contentWindow_.postMessage([type, data], '*');
  }

  /** @override */
  frameCreated(frame) {
    this.postMessage_('frameCreated', frame);
  }

  /** @override */
  pageCreated(page) {
    this.postMessage_('pageCreated', page);
  }

  /** @override */
  processCreated(process) {
    this.postMessage_('processCreated', process);
  }

  /** @override */
  workerCreated(worker) {
    this.postMessage_('workerCreated', worker);
  }

  /** @override */
  frameChanged(frame) {
    this.postMessage_('frameChanged', frame);
  }

  /** @override */
  pageChanged(page) {
    this.postMessage_('pageChanged', page);
  }

  /** @override */
  processChanged(process) {
    this.postMessage_('processChanged', process);
  }

  /** @override */
  workerChanged(worker) {
    this.postMessage_('workerChanged', worker);
  }

  /** @override */
  favIconDataAvailable(icon_info) {
    this.postMessage_('favIconDataAvailable', icon_info);
  }

  /** @override */
  nodeDeleted(nodeId) {
    this.postMessage_('nodeDeleted', nodeId);
  }
}

Polymer({
  is: 'graph-tab',

  _template: html`{__html_template__}`,

  /**
   * The Mojo graph data source.
   *
   * @private {discards.mojom.GraphDumpRemote}
   */
  graphDump_: null,

  /**
   * The graph change listener.
   *
   * @private {discards.mojom.GraphChangeStreamInterface}
   */
  changeListener_: null,

  /**
   * The WebView's content window object.
   * @private {?Window}
   */
  contentWindow_: null,

  /** @override */
  ready() {
    this.graphDump_ = discards.mojom.GraphDump.getRemote();
  },

  /** @override */
  detached() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    this.changeListener_ = null;
  },

  /**
   * @param {!Event} event A request from the WebView.
   * @private
   */
  onMessage_(event) {
    const type = /** @type {string} */ (event.data[0]);
    const data = /** @type {Object|number} */ (event.data[1]);
    switch (type) {
      case 'requestNodeDescriptions':
        // Forward the request through the mojoms and bounce the reply back.
        this.graphDump_
            .requestNodeDescriptions(/** @type {!Array<number>} */ (data))
            .then(
                (descriptions) => this.contentWindow_.postMessage(
                    ['nodeDescriptions', descriptions.nodeDescriptionsJson],
                    '*'));
        break;
    }
  },

  /** @private */
  onWebViewReady_() {
    this.contentWindow_ = this.$.webView.contentWindow;
    this.changeListener_ =
        new DiscardsGraphChangeStreamImpl(this.contentWindow_);
    this.client_ =
        new discards.mojom.GraphChangeStreamReceiver(this.changeListener_);
    // Subscribe for graph updates.
    this.graphDump_.subscribeToChanges(
        this.client_.$.bindNewPipeAndPassRemote());

    window.addEventListener('message', this.onMessage_.bind(this));
  },
});
