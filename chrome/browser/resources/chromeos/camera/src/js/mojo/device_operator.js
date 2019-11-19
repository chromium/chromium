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
 * import {Resolution} from '../type.js';
 */
var Resolution = Resolution || {};

/**
 * Operates video capture device through CrOS Camera App Mojo interface.
 */
cca.mojo.DeviceOperator = class {
  /**
   * @public
   */
  constructor() {
    /**
     * An interface remote that is used to construct the mojo interface.
     * @type {cros.mojom.CameraAppDeviceProviderRemote}
     * @private
     */
    this.deviceProvider_ = cros.mojom.CameraAppDeviceProvider.getRemote();

    /**
     * Flag that indicates if the direct communication between camera app and
     * video capture devices is supported.
     * @type {!Promise<boolean>}
     * @private
     */
    this.isSupported_ =
        this.deviceProvider_.isSupported().then(({isSupported}) => {
          return isSupported;
        });
  }

  /**
   * Gets corresponding device remote by given id.
   * @param {string} deviceId The id of target camera device.
   * @return {!Promise<!cros.mojom.CameraAppDeviceRemote>} Corresponding device
   *     remote.
   * @throws {Error} Thrown when given device id is invalid.
   */
  async getDevice_(deviceId) {
    const {device, status} =
        await this.deviceProvider_.getCameraAppDevice(deviceId);
    if (status === cros.mojom.GetCameraAppDeviceStatus.ERROR_INVALID_ID) {
      throw new Error('Invalid device id: ', deviceId);
    }
    if (device === null) {
      throw new Error('Unknown error');
    }
    return device;
  }

  /**
   * Gets supported photo resolutions for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!ResolutionList>} Promise of supported resolutions.
   * @throws {Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getPhotoResolutions(deviceId) {
    const formatBlob = 33;
    const typeOutputStream = 0;
    const numElementPerEntry = 4;

    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    const staticMetadata = cameraInfo.staticCameraCharacteristics;
    const streamConfigs = cca.mojo.getMetadataData_(
        staticMetadata,
        cros.mojom.CameraMetadataTag
            .ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    // The data of |streamConfigs| looks like:
    // streamConfigs: [FORMAT_1, WIDTH_1, HEIGHT_1, TYPE_1,
    //                 FORMAT_2, WIDTH_2, HEIGHT_2, TYPE_2, ...]
    if (streamConfigs.length % numElementPerEntry !== 0) {
      throw new Error('Unexpected length of stream configurations');
    }

    const supportedResolutions = [];
    for (let i = 0; i < streamConfigs.length; i += numElementPerEntry) {
      const [format, width, height, type] =
          streamConfigs.slice(i, i + numElementPerEntry);
      if (format === formatBlob && type === typeOutputStream) {
        supportedResolutions.push(new Resolution(width, height));
      }
    }
    return supportedResolutions;
  }

  /**
   * Gets supported video configurations for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!Array<VideoConfig>>} Promise of supported video
   *     configurations.
   * @throws {Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getVideoConfigs(deviceId) {
    // Currently we use YUV format for both recording and previewing on Chrome.
    const formatYuv = 35;
    const oneSecondInNs = 1e9;
    const numElementPerEntry = 4;

    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    const staticMetadata = cameraInfo.staticCameraCharacteristics;
    const minFrameDurationConfigs = cca.mojo.getMetadataData_(
        staticMetadata,
        cros.mojom.CameraMetadataTag
            .ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
    // The data of |minFrameDurationConfigs| looks like:
    // minFrameDurationCOnfigs: [FORMAT_1, WIDTH_1, HEIGHT_1, DURATION_1,
    //                           FORMAT_2, WIDTH_2, HEIGHT_2, DURATION_2,
    //                           ...]
    if (minFrameDurationConfigs.length % numElementPerEntry !== 0) {
      throw new Error('Unexpected length of frame durations configs');
    }

    const supportedConfigs = [];
    for (let i = 0; i < minFrameDurationConfigs.length;
         i += numElementPerEntry) {
      const [format, width, height, minDuration] =
          minFrameDurationConfigs.slice(i, i + numElementPerEntry);
      if (format === formatYuv) {
        const maxFps = Math.round(oneSecondInNs / minDuration);
        supportedConfigs.push({width, height, maxFps});
      }
    }
    return supportedConfigs;
  }

  /**
   * Gets camera facing for given device.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!cros.mojom.CameraFacing>} Promise of device facing.
   * @throws {Error} Thrown when the device operation is not supported.
   */
  async getCameraFacing(deviceId) {
    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    return cameraInfo.facing;
  }

  /**
   * Gets supported fps ranges for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!FpsRangeList>} Promise of supported fps ranges.
   *     Each range is represented as [min, max].
   * @throws {Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getSupportedFpsRanges(deviceId) {
    const numElementPerEntry = 2;

    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    const staticMetadata = cameraInfo.staticCameraCharacteristics;
    const availableFpsRanges = cca.mojo.getMetadataData_(
        staticMetadata,
        cros.mojom.CameraMetadataTag
            .ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
    // The data of |availableFpsRanges| looks like:
    // availableFpsRanges: [RANGE_1_MIN, RANGE_1_MAX,
    //                      RANGE_2_MIN, RANGE_2_MAX, ...]
    if (availableFpsRanges.length % numElementPerEntry !== 0) {
      throw new Error('Unexpected length of available fps range configs');
    }

    const /** !FpsRangeList */ supportedFpsRanges = [];
    for (let i = 0; i < availableFpsRanges.length; i += numElementPerEntry) {
      const [minFps, maxFps] =
          availableFpsRanges.slice(i, i + numElementPerEntry);
      supportedFpsRanges.push({minFps, maxFps});
    }
    return supportedFpsRanges;
  }

  /**
   * Sets the fps range for target device.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @param {MediaStreamConstraints} constraints The constraints including fps
   *     range and the resolution. If frame rate range negotiation is needed,
   *     the caller should either set exact field or set both min and max fields
   *     for frame rate property.
   * @throws {Error} Thrown when the input contains invalid values or the device
   *     operation is not supported.
   */
  async setFpsRange(deviceId, constraints) {
    let /** number */ streamWidth = 0;
    let /** number */ streamHeight = 0;
    let /** number */ minFrameRate = 0;
    let /** number */ maxFrameRate = 0;

    if (constraints && constraints.video && constraints.video.frameRate) {
      const frameRate = constraints.video.frameRate;
      if (frameRate.exact) {
        minFrameRate = frameRate.exact;
        maxFrameRate = frameRate.exact;
      } else if (frameRate.min && frameRate.max) {
        minFrameRate = frameRate.min;
        maxFrameRate = frameRate.max;
      }
      // TODO(wtlee): To set the fps range to the default value, we should
      // remove the frameRate from constraints instead of using incomplete
      // range.

      // We only support number type for width and height. If width or height
      // is other than a number (e.g. ConstrainLong, undefined, etc.), we should
      // throw an error.
      if (typeof constraints.video.width !== 'number') {
        throw new Error('width in constraints is expected to be a number');
      }
      streamWidth = constraints.video.width;

      if (typeof constraints.video.height !== 'number') {
        throw new Error('height in constraints is expected to be a number');
      }
      streamHeight = constraints.video.height;
    }

    const hasSpecifiedFrameRateRange = minFrameRate > 0 && maxFrameRate > 0;
    // If the frame rate range is specified in |constraints|, we should try to
    // set the frame rate range and should report error if fails since it is
    // unexpected.
    //
    // Otherwise, if the frame rate is incomplete or totally missing in
    // |constraints| , we assume the app wants to use default frame rate range.
    // We set the frame rate range to an invalid range (e.g. 0 fps) so that it
    // will fallback to use the default one.
    const device = await this.getDevice_(deviceId);
    const {isSuccess} = await device.setFpsRange(
        {'width': streamWidth, 'height': streamHeight},
        {'start': minFrameRate, 'end': maxFrameRate});

    if (!isSuccess && hasSpecifiedFrameRateRange) {
      console.error('Failed to negotiate the frame rate range.');
    }
  }

  /**
   * Sets the intent for the upcoming capture session.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @param {cros.mojom.CaptureIntent} captureIntent The purpose of this
   *     capture, to help the camera device decide optimal configurations.
   * @return {!Promise} Promise for the operation.
   */
  async setCaptureIntent(deviceId, captureIntent) {
    const device = await this.getDevice_(deviceId);
    await device.setCaptureIntent(captureIntent);
  }

  /**
   * Checks if portrait mode is supported.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<boolean>} Promise of the boolean result.
   * @throws {Error} Thrown when the device operation is not supported.
   */
  async isPortraitModeSupported(deviceId) {
    // TODO(wtlee): Change to portrait mode tag.
    // This should be 0x80000000 but mojo interface will convert the tag to
    // int32.
    const portraitModeTag =
        /** @type{cros.mojom.CameraMetadataTag} */ (-0x80000000);

    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    return cca.mojo
               .getMetadataData_(
                   cameraInfo.staticCameraCharacteristics, portraitModeTag)
               .length > 0;
  }

  /**
   * Adds a metadata observer to Camera App Device through Mojo IPC.
   * @param {string} deviceId The id for target camera device.
   * @param {!function(cros.mojom.CameraMetadata)} callback Callback that
   *     handles the metadata.
   * @param {cros.mojom.StreamType} streamType Stream type which the observer
   *     gets the metadata from.
   * @return {!Promise<number>} id for the added observer. Can be used later
   *     to identify and remove the inserted observer.
   * @throws {Error} if fails to construct device connection.
   */
  async addMetadataObserver(deviceId, callback, streamType) {
    const observerCallbackRouter =
        new cros.mojom.ResultMetadataObserverCallbackRouter();
    observerCallbackRouter.onMetadataAvailable.addListener(callback);

    const device = await this.getDevice_(deviceId);
    const {id} = await device.addResultMetadataObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote(), streamType);
    return id;
  }

  /**
   * Remove a metadata observer from Camera App Device. A metadata observer
   * is recognized by its id returned by addMetadataObserver upon insertion.
   * @param {string} deviceId The id for target camera device.
   * @param {!number} observerId The id for the metadata observer to be removed.
   * @return {!Promise<boolean>} Promise for the result. It will be resolved
   *     with a boolean indicating whether the removal is successful or not.
   * @throws {Error} if fails to construct device connection.
   */
  async removeMetadataObserver(deviceId, observerId) {
    const device = await this.getDevice_(deviceId);
    const {isSuccess} = await device.removeResultMetadataObserver(observerId);
    return isSuccess;
  }

  /**
   * Adds observer to observe shutter event.
   *
   * The shutter event is defined as CAMERA3_MSG_SHUTTER in
   * media/capture/video/chromeos/mojom/camera3.mojom which will be sent from
   * underlying camera HAL after sensor finishes frame capturing.
   *
   * @param {string} deviceId The id for target camera device.
   * @param {!function()} callback Callback to trigger on shutter done.
   * @return {!Promise<number>} Id for the added observer.
   * @throws {Error} if fails to construct device connection.
   */
  async addShutterObserver(deviceId, callback) {
    const observerCallbackRouter =
        new cros.mojom.CameraEventObserverCallbackRouter();
    observerCallbackRouter.onShutterDone.addListener(callback);

    const device = await this.getDevice_(deviceId);
    const {id} = await device.addCameraEventObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    return id;
  }

  /**
   * Removes a shutter observer from Camera App Device.
   * @param {string} deviceId The id of target camera device.
   * @param {!number} observerId The id of the observer to be removed.
   * @return {!Promise<boolean>} True when the observer is successfully removed.
   * @throws {Error} if fails to construct device connection.
   */
  async removeShutterObserver(deviceId, observerId) {
    const device = await this.getDevice_(deviceId);
    const {isSuccess} = await device.removeCameraEventObserver(observerId);
    return isSuccess;
  }

  /**
   * Sets reprocess option which is normally an effect to the video capture
   * device before taking picture.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @param {!cros.mojom.Effect} effect The target reprocess option (effect)
   *     that would be applied on the result.
   * @return {!Promise<!media.mojom.Blob>} The captured
   *     result with given effect.
   * @throws {Error} Thrown when the reprocess is failed or the device operation
   *     is not supported.
   */
  async setReprocessOption(deviceId, effect) {
    const device = await this.getDevice_(deviceId);
    const {status, blob} = await device.setReprocessOption(effect);
    if (blob === null) {
      throw new Error('Set reprocess failed: ' + status);
    }
    return blob;
  }

  /**
   * Creates a new instance of DeviceOperator if it is not set. Returns the
   *     exist instance.
   * @return {!Promise<?cca.mojo.DeviceOperator>} The singleton instance.
   */
  static async getInstance() {
    if (this.instance_ === null) {
      this.instance_ = new cca.mojo.DeviceOperator();
    }
    if (!await this.instance_.isSupported_) {
      return null;
    }
    return this.instance_;
  }

  /**
   * Gets if DeviceOperator is supported.
   * @return {!Promise<boolean>} True if the DeviceOperator is supported.
   */
  static async isSupported() {
    return await this.getInstance() !== null;
  }
};

/**
 * The singleton instance of DeviceOperator. Initialized by the first
 * invocation of getInstance().
 * @type {?cca.mojo.DeviceOperator}
 */
cca.mojo.DeviceOperator.instance_ = null;

/**
 * Gets the data from Camera metadata by its tag.
 * @param {!cros.mojom.CameraMetadata} metadata Camera metadata from which to
 *     query the data.
 * @param {!cros.mojom.CameraMetadataTag} tag Camera metadata tag to query for.
 * @return {!Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|. If nothing is found, returns an empty
 *     array.
 * @private
 */
cca.mojo.getMetadataData_ = function(metadata, tag) {
  for (let i = 0; i < metadata.entryCount; i++) {
    const entry = metadata.entries[i];
    if (entry.tag === tag) {
      return cca.mojo.parseMetadataData(entry);
    }
  }
  return [];
};

/**
 * Parse the entry data according to its type.
 * @param {!cros.mojom.CameraMetadataEntry} entry Camera metadata entry
 *     from which to parse the data according to its type.
 * @return {!Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|.
 * @throws {Error} if entry type is not supported.
 */
cca.mojo.parseMetadataData = function(entry) {
  const {buffer} = Uint8Array.from(entry.data);
  switch (entry.type) {
    case cros.mojom.EntryType.TYPE_BYTE:
      return Array.from(new Uint8Array(buffer));
    case cros.mojom.EntryType.TYPE_INT32:
      return Array.from(new Int32Array(buffer));
    case cros.mojom.EntryType.TYPE_FLOAT:
      return Array.from(new Float32Array(buffer));
    case cros.mojom.EntryType.TYPE_DOUBLE:
      return Array.from(new Float64Array(buffer));
    case cros.mojom.EntryType.TYPE_INT64:
      return Array.from(new BigInt64Array(buffer), (bigIntVal) => {
        const numVal = Number(bigIntVal);
        if (!Number.isSafeInteger(numVal)) {
          console.warn('The int64 value is not a safe integer');
        }
        return numVal;
      });
    case cros.mojom.EntryType.TYPE_RATIONAL: {
      const arr = new Int32Array(buffer);
      const values = [];
      for (let i = 0; i < arr.length; i += 2) {
        values.push(arr[i] / arr[i + 1]);
      }
      return values;
    }
    default:
      throw new Error('Unsupported type: ' + entry.type);
  }
};
