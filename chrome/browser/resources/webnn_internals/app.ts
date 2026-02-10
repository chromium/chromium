// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {WebNNInternalsHandlerFactory, WebNNInternalsHandlerRemote, WebNNInternalsPageReceiver} from './webnn_internals.mojom-webui.js';
import type {WebNNInternalsPageInterface} from './webnn_internals.mojom-webui.js';

class WebNnInternalsApp implements WebNNInternalsPageInterface {
  private handler: WebNNInternalsHandlerRemote;
  private receiver: WebNNInternalsPageReceiver;

  constructor() {
    this.handler = new WebNNInternalsHandlerRemote();
    this.receiver = new WebNNInternalsPageReceiver(this);
    const factory = WebNNInternalsHandlerFactory.getRemote();
    factory.createWebNNInternalsHandler(
        this.receiver.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver(),
    );

    this.setupGraphRecordingController();
  }

  async setupGraphRecordingController() {
// <if expr="not webnn_enable_graph_dump">
// Dummy await to bypass '@typescript-eslint/require-await' lint error when
// 'webnn_enable_graph_dump' is false.
    await Promise.resolve();
// </if>

    const statusElement = document.getElementById(
                              'record-graph-status',
                              ) as HTMLElement;
    assert(statusElement, 'record-graph-status element not found');

    statusElement.textContent =
        'This feature only works under build flag webnn_enable_graph_dump=true';

// <if expr="webnn_enable_graph_dump">
    statusElement.textContent = 'Connected';
// </if>

    const recordGraphCheckbox = document.getElementById(
                                    'record-graph-checkbox',
                                    ) as HTMLInputElement;
    assert(recordGraphCheckbox, 'record-graph-checkbox element not found');

    recordGraphCheckbox.disabled = true;

// <if expr="webnn_enable_graph_dump">
    const response = await this.handler.isGraphRecording();
    recordGraphCheckbox.checked = response.enabled;
    recordGraphCheckbox.disabled = false;

    recordGraphCheckbox.addEventListener('change', (event) => {
      this.handler.setGraphRecordEnabled(
          (event.target as HTMLInputElement).checked,
      );
    });
// </if>
  }

  exportGraphRecorded(jsonData: BigBuffer) {
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

  onGraphRecordEnabledChanged(isEnabled: boolean) {
    const recordGraphCheckbox = document.getElementById(
                                    'record-graph-checkbox',
                                    ) as HTMLInputElement;
    if (recordGraphCheckbox) {
      recordGraphCheckbox.checked = isEnabled;
    }
  }
}

document.addEventListener('DOMContentLoaded', () => {
  new WebNnInternalsApp();
});
