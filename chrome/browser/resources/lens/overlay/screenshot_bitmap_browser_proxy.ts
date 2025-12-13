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

type ScreenshotReceivedCallback =
    (screenshotBitmap: ImageBitmap, isSidePanelOpen: boolean) => void;
type OverlayReshownCallback = (screenshotBitmap: ImageBitmap) => void;

export interface ScreenshotBitmapBrowserProxy {
  // Returns the screenshot from the browser process. If the screenshot has been
  // sent already, the promise will return immediately. Else, the promise will
  // resolve once the screenshot has been retrieved.
  fetchScreenshot(callback: ScreenshotReceivedCallback): void;
  addOnOverlayReshownListener(callback: OverlayReshownCallback): void;
}

export class ScreenshotBitmapBrowserProxyImpl implements
    ScreenshotBitmapBrowserProxy {
  private screenshot?: ImageBitmap;
  private isSidePanelOpenOnScreenshot: boolean = false;
  private screenshotListenerId: number;
  private onOverlayReshownListenerId: number;
  private callbacks: ScreenshotReceivedCallback[] = [];
  private onOverlayReshownCallbacks: OverlayReshownCallback[] = [];

  constructor() {
    this.screenshotListenerId =
        BrowserProxyImpl.getInstance()
            .callbackRouter.screenshotDataReceived.addListener(
                this.screenshotDataReceived.bind(this));
    this.onOverlayReshownListenerId =
        BrowserProxyImpl.getInstance()
            .callbackRouter.onOverlayReshown.addListener(
                this.onOverlayReshown.bind(this));
  }

  static getInstance(): ScreenshotBitmapBrowserProxy {
    return instance || (instance = new ScreenshotBitmapBrowserProxyImpl());
  }

  static setInstance(obj: ScreenshotBitmapBrowserProxy) {
    instance = obj;
  }

  fetchScreenshot(callback: ScreenshotReceivedCallback): void {
    // Store the callback for calling if the screenshot updates or when the
    // screenshot is ready if the below check fails.
    this.callbacks.push(callback);

    if (this.screenshot) {
      // We need to make a new bitmap because each canvas takes ownership of the
      // bitmap, so it cannot be drawn to multiple HTMLCanvasElement.
      createImageBitmap(this.screenshot).then((bitmap) => {
        callback(bitmap, this.isSidePanelOpenOnScreenshot);
      });
      return;
    }
  }

  addOnOverlayReshownListener(callback: OverlayReshownCallback): void {
    this.onOverlayReshownCallbacks.push(callback);
  }

  private async parseScreenshotData(
      screenshotData: BitmapMappedFromTrustedProcess): Promise<ImageBitmap> {
    const data: BigBuffer = screenshotData.pixelData;
    // Pull the pixel data into a Uint8ClampedArray.
    let pixelData: Uint8ClampedArray<ArrayBuffer>;
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
    return createImageBitmap(imageData);
  }

  private async onOverlayReshown(
      screenshotData: BitmapMappedFromTrustedProcess) {
    const data: BigBuffer = screenshotData.pixelData;
    // TODO(crbug.com/334185985): This occurs when the browser failed to
    // allocate the memory for the pixels. Handle this case.
    if (data.invalidBuffer) {
      return;
    }

    this.screenshot = await this.parseScreenshotData(screenshotData);
    // Send the screenshot to all the callbacks.
    for (const callback of this.onOverlayReshownCallbacks) {
      // We need to make a new bitmap because each canvas takes ownership of the
      // bitmap, so it cannot be drawn to multiple HTMLCanvasElement.
      createImageBitmap(this.screenshot).then((bitmap) => {
        callback(bitmap);
      });
    }
  }

  private async screenshotDataReceived(
      screenshotData: BitmapMappedFromTrustedProcess,
      isSidePanelOpen: boolean) {
    const data: BigBuffer = screenshotData.pixelData;
    // TODO(crbug.com/334185985): This occurs when the browser failed to
    // allocate the memory for the pixels. Handle this case.
    if (data.invalidBuffer) {
      return;
    }

    // Cache the bitmap for future requests
    this.screenshot = await this.parseScreenshotData(screenshotData);
    this.isSidePanelOpenOnScreenshot = isSidePanelOpen;

    // Send the screenshot to all the callbacks.
    for (const callback of this.callbacks) {
      // We need to make a new bitmap because each canvas takes ownership of the
      // bitmap, so it cannot be drawn to multiple HTMLCanvasElement.
      createImageBitmap(this.screenshot).then((bitmap) => {
        callback(bitmap, this.isSidePanelOpenOnScreenshot);
      });
    }
  }
}
