// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';
import * as Comlink from './lib/comlink.js';

/**
 * The MP4 video processor URL in trusted type.
 */
const mp4VideoProcessorURL: TrustedScriptURL = (() => {
  const staticUrlPolicy = assertExists(window.trustedTypes)
                              .createPolicy('video-processor-js-static', {
                                createScriptURL: (_url: string) =>
                                    '/js/models/ffmpeg/video_processor.js',
                              });
  // TODO(crbug.com/980846): Remove the empty string if
  // https://github.com/w3c/webappsec-trusted-types/issues/278 gets fixed.
  return staticUrlPolicy.createScriptURL('');
})();

/**
 * Connects the |port| to worker which exposes the video processor.
 */
async function connectToWorker(port: MessagePort): Promise<void> {
  /**
   * TODO(pihsun): TypeScript only supports string|URL instead of
   * TrustedScriptURL as parameter to Worker.
   */
  const trustedURL = mp4VideoProcessorURL as unknown as URL;

  // TODO(pihsun): actually get correct type from the function definition.
  const worker = Comlink.wrap<{exposeVideoProcessor(port: MessagePort): void}>(
      new Worker(trustedURL, {type: 'module'}));
  await worker.exposeVideoProcessor(Comlink.transfer(port, [port]));
}

export interface VideoProcessorHelper {
  connectToWorker: typeof connectToWorker;
}
export {connectToWorker};
