// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';

/**
 * @fileoverview A browser proxy for receiving the viewport screenshot from the
 * browser.
 */
let instance: ScreenshotBitmapBrowserProxy|null = null;

type ScreenshotReceivedCallback = (screenshotBitmap: ImageBitmap) => void;

export interface ScreenshotBitmapBrowserProxy {
  // Returns the screenshot from the browser process. If the screenshot has been
  // sent already, the promise will return immediately. Else, the promise will
  // resolve once the screenshot has been retrieved.
  fetchScreenshot(callback: ScreenshotReceivedCallback): void;
}

export class ScreenshotBitmapBrowserProxyImpl implements
    ScreenshotBitmapBrowserProxy {
  private screenshot?: ImageBitmap;
  private screenshotListenerId: number;
  private callbacks: ScreenshotReceivedCallback[] = [];

  constructor() {
    this.screenshotListenerId =
        BrowserProxyImpl.getInstance()
            .callbackRouter.screenshotDataReceived.addListener(
                this.screenshotDataReceived.bind(this));
  }

  static getInstance(): ScreenshotBitmapBrowserProxy {
    return instance || (instance = new ScreenshotBitmapBrowserProxyImpl());
  }

  static setInstance(obj: ScreenshotBitmapBrowserProxy) {
    instance = obj;
  }

  fetchScreenshot(callback: ScreenshotReceivedCallback): void {
    if (this.screenshot) {
      // We need to make a new bitmap because each canvas takes ownership of the
      // bitmap, so it cannot be drawn to multiple HTMLCanvasElement.
      createImageBitmap(this.screenshot).then((bitmap) => {
        callback(bitmap);
      });
      return;
    }

    // Queue the callback for when the screenshot is ready.
    this.callbacks.push(callback);
  }

  private async screenshotDataReceived(screenshotData:
                                           BitmapMappedFromTrustedProcess) {
    const data: BigBuffer = screenshotData.pixelData;

    // TODO(b/334185985): This occurs when the browser failed to allocate the
    // memory for the pixels. Handle this case.
    if (data.invalidBuffer) {
      return;
    }

    // Pull the pixel data into a Uint8ClampedArray.
    let pixelData: Uint8ClampedArray;
    if (Array.isArray(data.bytes)) {
      pixelData = new Uint8ClampedArray(data.bytes);
    } else {
      // If the buffer is not invalid or an array, it must be shared memory.
      assert(data.sharedMemory);
      const sharedMemory = data.sharedMemory;
      const {buffer, result} =
          sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
      assert(result === Mojo.RESULT_OK);
      pixelData = new Uint8ClampedArray(buffer);
    }

    const imageWidth = screenshotData.imageInfo.width;
    const imageHeight = screenshotData.imageInfo.height;

    // Put our screenshot into ImageData object so it can be rendered in a
    // Canvas.
    const imageData = new ImageData(pixelData, imageWidth, imageHeight);
    const imageBitmap = await createImageBitmap(imageData);

    // Cache the bitmap for future requests
    this.screenshot = imageBitmap;

    // Send the screenshot to all the callbacks.
    for (const callback of this.callbacks) {
      // We need to make a new bitmap because each canvas takes ownership of the
      // bitmap, so it cannot be drawn to multiple HTMLCanvasElement.
      createImageBitmap(this.screenshot).then((bitmap) => {
        callback(bitmap);
      });
    }
    this.callbacks = [];

    // Stop listening for new screenshots.
    assert(BrowserProxyImpl.getInstance().callbackRouter.removeListener(
        this.screenshotListenerId));
  }
}
