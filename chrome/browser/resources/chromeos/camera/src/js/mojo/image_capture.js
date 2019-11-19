// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for mojo.
 */
cca.mojo = cca.mojo || {};

/**
 * Type definition for cca.mojo.PhotoCapabilities.
 * @extends {PhotoCapabilities}
 * @record
 */
cca.mojo.PhotoCapabilities = function() {};

/** @type {Array<string>} */
cca.mojo.PhotoCapabilities.prototype.supportedEffects;

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 * @param {!MediaStreamTrack} videoTrack A video track whose still images will
 *     be taken.
 * @constructor
 */
cca.mojo.ImageCapture = function(videoTrack) {
  /**
   * The id of target media device.
   * @type {string}
   * @private
   */
  this.deviceId_ = videoTrack.getSettings().deviceId;

  /**
   * The standard ImageCapture object.
   * @type {ImageCapture}
   * @private
   */
  this.capture_ = new ImageCapture(videoTrack);

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets the photo capabilities with the available options/effects.
 * @return {!Promise<!PhotoCapabilities|cca.mojo.PhotoCapabilities>} Promise for
 *     the result.
 */
cca.mojo.ImageCapture.prototype.getPhotoCapabilities = async function() {
  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
  if (!deviceOperator) {
    return this.capture_.getPhotoCapabilities();
  }

  const supportedEffects = [cros.mojom.Effect.NO_EFFECT];
  const isPortraitModeSupported =
      await deviceOperator.isPortraitModeSupported(this.deviceId_);
  if (isPortraitModeSupported) {
    supportedEffects.push(cros.mojom.Effect.PORTRAIT_MODE);
  }
  const baseCapabilities = await this.capture_.getPhotoCapabilities();

  let /** !cca.mojo.PhotoCapabilities */ extendedCapabilities;
  Object.assign(extendedCapabilities, baseCapabilities, {supportedEffects});
  return extendedCapabilities;
};

/**
 * Takes single or multiple photo(s) with the specified settings and effects.
 * The amount of result photo(s) depends on the specified settings and effects,
 * and the first promise in the returned array will always resolve with the
 * unreprocessed photo. The returned array will be resolved once it received
 * the shutter event.
 * @param {!PhotoSettings} photoSettings Photo settings for ImageCapture's
 *     takePhoto().
 * @param {!Array<cros.mojom.Effect>=} photoEffects Photo effects to be applied.
 * @return {!Promise<!Array<!Promise<!Blob>>>} A promise of the array containing
 *     promise of each blob result.
 */
cca.mojo.ImageCapture.prototype.takePhoto =
    async function(photoSettings, photoEffects = []) {
  /** @type {Array<!Promise<!Blob>>} */
  const takes = [];
  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
  if (deviceOperator === null && photoEffects.length > 0) {
    throw new Error('Applying effects is not supported on this device');
  }

  for (const effect of photoEffects) {
    const take = (async () => {
      const {data, mimeType} =
          await deviceOperator.setReprocessOption(this.deviceId_, effect);
      return new Blob([new Uint8Array(data)], {type: mimeType});
    })();
    takes.push(take);
  }

  if (deviceOperator !== null) {
    let onShutterDone;
    const isShutterDone = new Promise((resolve) => {
      onShutterDone = resolve;
    });
    const observerId =
        await deviceOperator.addShutterObserver(this.deviceId_, onShutterDone);
    takes.unshift(this.capture_.takePhoto(photoSettings));
    await isShutterDone;
    const isSuccess =
        await deviceOperator.removeShutterObserver(this.deviceId_, observerId);
    if (!isSuccess) {
      console.error('Failed to remove shutter observer');
    }
    return takes;
  } else {
    takes.unshift(this.capture_.takePhoto(photoSettings));
    return takes;
  }
};
