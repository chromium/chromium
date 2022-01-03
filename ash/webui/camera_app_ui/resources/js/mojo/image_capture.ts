// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bitmapToJpegBlob} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {DeviceOperator} from './device_operator.js';
import {Effect} from './type.js';
import {closeEndpoint} from './util.js';

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 */
export class CrosImageCapture {
  /**
   * The id of target media device.
   */
  private readonly deviceId: string;

  /**
   * The standard ImageCapture object.
   */
  private readonly capture: ImageCapture;

  /**
   * @param videoTrack A video track whose still images will be taken.
   */
  constructor(videoTrack: MediaStreamTrack) {
    this.deviceId = videoTrack.getSettings().deviceId;
    this.capture = new ImageCapture(videoTrack);
  }

  /**
   * Gets the photo capabilities with the available options/effects.
   * @return Promise for the result.
   */
  async getPhotoCapabilities(): Promise<PhotoCapabilities> {
    return this.capture.getPhotoCapabilities();
  }

  /**
   * Takes single or multiple photo(s) with the specified settings and effects.
   * The amount of result photo(s) depends on the specified settings and
   * effects, and the first promise in the returned array will always resolve
   * with the unreprocessed photo. The returned array will be resolved once it
   * received the shutter event.
   * @param photoSettings Photo settings for ImageCapture's takePhoto().
   * @param photoEffects Photo effects to be applied.
   * @return A promise of the array containing promise of each blob result.
   */
  async takePhoto(photoSettings: PhotoSettings, photoEffects: Effect[] = []):
      Promise<Array<Promise<Blob>>> {
    const deviceOperator = await DeviceOperator.getInstance();
    if (deviceOperator === null && photoEffects.length > 0) {
      throw new Error('Applying effects is not supported on this device');
    }

    const takes =
        await deviceOperator.setReprocessOptions(this.deviceId, photoEffects);
    if (deviceOperator !== null) {
      const onShutterDone = new WaitableEvent();
      const shutterObserver =
          await deviceOperator.addShutterObserver(this.deviceId, () => {
            onShutterDone.signal();
          });
      takes.unshift(this.capture.takePhoto(photoSettings));
      await onShutterDone.wait();
      closeEndpoint(shutterObserver);
      return takes;
    } else {
      takes.unshift(this.capture.takePhoto(photoSettings));
      return takes;
    }
  }

  grabFrame(): Promise<ImageBitmap> {
    return this.capture.grabFrame();
  }

  /**
   * @return Returns jpeg blob of the grabbed frame.
   */
  async grabJpegFrame(): Promise<Blob> {
    const bitmap = await this.capture.grabFrame();
    return bitmapToJpegBlob(bitmap);
  }
}
