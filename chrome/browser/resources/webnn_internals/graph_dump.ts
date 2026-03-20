// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './graph_dump.css.js';
import {getHtml} from './graph_dump.html.js';

export class WebnnInternalsGraphDumpElement extends CrLitElement {
  static get is() {
    return 'webnn-internals-graph-dump';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      recordGraphEnabled_: {type: Boolean},
    };
  }

  private listenerIds: number[] = [];
  protected accessor recordGraphEnabled_: boolean = false;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.updateRecordingStatus();
    this.listenerIds = [
      this.proxy_.callbackRouter.onGraphRecordEnabledChanged.addListener(
          this.onGraphRecordEnabledChanged_.bind(this)),
      this.proxy_.callbackRouter.exportGraphRecorded.addListener(
          this.exportGraphRecorded_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds = [];
  }

  private async updateRecordingStatus() {
    const response = await this.proxy_.handler.isGraphRecording();
    this.recordGraphEnabled_ = response.enabled;
  }

  protected onToggleValueChange_(event: CustomEvent<boolean>) {
    if (this.recordGraphEnabled_ === event.detail) {
      return;
    }
    this.proxy_.handler.setGraphRecordEnabled(event.detail);
  }

  private onGraphRecordEnabledChanged_(isEnabled: boolean) {
    this.recordGraphEnabled_ = isEnabled;
  }

  private exportGraphRecorded_(jsonData: BigBuffer) {
    let bytes: Uint8Array<ArrayBuffer>|null = null;
    if (jsonData.bytes) {
      bytes = new Uint8Array(jsonData.bytes);
    } else if (jsonData.sharedMemory) {
      const {result, buffer} = jsonData.sharedMemory.bufferHandle.mapBuffer(
          0,
          jsonData.sharedMemory.size,
      );
      assert(result === Mojo.RESULT_OK, 'Failed to map shared memory');
      bytes = new Uint8Array(buffer, 0, jsonData.sharedMemory.size);
    }

    if (bytes === null) {
      return;
    }

    const blob = new Blob([bytes], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `webnn_graph_${Date.now()}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webnn-internals-graph-dump': WebnnInternalsGraphDumpElement;
  }
}

customElements.define(
    WebnnInternalsGraphDumpElement.is, WebnnInternalsGraphDumpElement);
