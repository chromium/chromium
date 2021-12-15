// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import {reportError} from '../error.js';
import {Point} from '../geometry.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  FpsRangeList,  // eslint-disable-line no-unused-vars
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
  VideoConfig,     // eslint-disable-line no-unused-vars
} from '../type.js';
import {WaitableEvent} from '../waitable_event.js';

import {
  AndroidInfoSupportedHardwareLevel,
  CameraAppDeviceProvider,
  CameraAppDeviceProviderRemote,  // eslint-disable-line no-unused-vars
  CameraAppDeviceRemote,          // eslint-disable-line no-unused-vars
  CameraEventObserverCallbackRouter,
  CameraFacing,
  CameraMetadata,       // eslint-disable-line no-unused-vars
  CameraMetadataEntry,  // eslint-disable-line no-unused-vars
  CameraMetadataTag,
  CaptureIntent,  // eslint-disable-line no-unused-vars
  DocumentCornersObserverCallbackRouter,
  Effect,  // eslint-disable-line no-unused-vars
  EntryType,
  GetCameraAppDeviceStatus,
  ReprocessResultListenerCallbackRouter,
  ResultMetadataObserverCallbackRouter,
  StreamType,  // eslint-disable-line no-unused-vars
} from './type.js';
import {
  closeEndpoint,
  MojoEndpoint,  // eslint-disable-line no-unused-vars
  wrapEndpoint,
} from './util.js';

/**
 * Parse the entry data according to its type.
 * @param {!CameraMetadataEntry} entry Camera metadata entry
 *     from which to parse the data according to its type.
 * @return {!Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|.
 * @throws {!Error} if entry type is not supported.
 */
export function parseMetadata(entry) {
  const {buffer} = Uint8Array.from(entry.data);
  switch (entry.type) {
    case EntryType.TYPE_BYTE:
      return Array.from(new Uint8Array(buffer));
    case EntryType.TYPE_INT32:
      return Array.from(new Int32Array(buffer));
    case EntryType.TYPE_FLOAT:
      return Array.from(new Float32Array(buffer));
    case EntryType.TYPE_DOUBLE:
      return Array.from(new Float64Array(buffer));
    case EntryType.TYPE_INT64:
      return Array.from(new BigInt64Array(buffer), (bigIntVal) => {
        const numVal = Number(bigIntVal);
        if (!Number.isSafeInteger(numVal)) {
          reportError(
              ErrorType.UNSAFE_INTEGER, ErrorLevel.WARNING,
              new Error('The int64 value is not a safe integer'));
        }
        return numVal;
      });
    case EntryType.TYPE_RATIONAL: {
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
}

/**
 * Gets the data from Camera metadata by its tag.
 * @param {!CameraMetadata} metadata Camera metadata from which to
 *     query the data.
 * @param {!CameraMetadataTag} tag Camera metadata tag to query for.
 * @return {!Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|. If nothing is found, returns an empty
 *     array.
 * @private
 */
function getMetadataData(metadata, tag) {
  for (let i = 0; i < metadata.entryCount; i++) {
    const entry = metadata.entries[i];
    if (entry.tag === tag) {
      return parseMetadata(entry);
    }
  }
  return [];
}

/**
 * The singleton instance of DeviceOperator. Initialized by the first
 * invocation of getInstance().
 * @type {?DeviceOperator}
 */
let instance = null;

/**
 * A ready event which should be signaled once the camera resource is ready.
 * @type {!WaitableEvent}
 */
const readyEvent = new WaitableEvent();

/**
 * Job queue to sequentialize devices operations.
 * @type {!AsyncJobQueue}
 */
const operationQueue = new AsyncJobQueue();

/**
 * Notified when the camera resource is ready.
 */
export function notifyCameraResourceReady() {
  readyEvent.signal();
}

/**
 * Operates video capture device through CrOS Camera App Mojo interface.
 */
export class DeviceOperator {
  /**
   * @public
   */
  constructor() {
    /**
     * An interface remote that is used to construct the mojo interface.
     * @type {!CameraAppDeviceProviderRemote}
     * @private
     */
    this.deviceProvider_ = wrapEndpoint(CameraAppDeviceProvider.getRemote());

    /**
     * Flag that indicates if the direct communication between camera app and
     * video capture devices is supported.
     * @type {!Promise<boolean>}
     * @private
     */
    this.isSupported_ = (async () => {
      const {isSupported} = await this.deviceProvider_.isSupported();
      return isSupported;
    })();

    /**
     * Map which maps from device id to the remote of devices. We want to have
     * only one remote for each devices to avoid unnecessary wastes of resources
     * and also makes it easier to control the connection.
     * @type {!Map<string, !Promise<!CameraAppDeviceRemote>>}
     * @private
     */
    this.devices_ = new Map();
  }

  /**
   * Gets corresponding device remote by given id.
   * @param {string} deviceId The id of target camera device.
   * @return {!Promise<!CameraAppDeviceRemote>} Corresponding device
   *     remote.
   * @throws {!Error} Thrown when given device id is invalid.
   */
  async getDevice_(deviceId) {
    const d = this.devices_.get(deviceId);
    if (d !== undefined) {
      return d;
    }
    const newDevice = (async () => {
      try {
        const {device, status} =
            await this.deviceProvider_.getCameraAppDevice(deviceId);
        if (status === GetCameraAppDeviceStatus.ERROR_INVALID_ID) {
          throw new Error(`Invalid device id`);
        }
        if (device === null) {
          throw new Error('Unknown error');
        }
        device.onConnectionError.addListener(() => {
          this.devices_.delete(deviceId);
        });
        return wrapEndpoint(device);
      } catch (e) {
        this.devices_.delete(deviceId);
        throw e;
      }
    })();
    this.devices_.set(deviceId, newDevice);
    return newDevice;
  }

  /**
   * Gets metadata for the given device from its static characteristics.
   * @param {string} deviceId The id of target camera device.
   * @param {!CameraMetadataTag} tag Camera metadata tag to query.
   * @return {!Promise<!Array<number>>} Promise of the corresponding data
   *     array.
   * @throws {!Error} Thrown when given device id is invalid.
   */
  async getStaticMetadata(deviceId, tag) {
    // All the unsigned vendor tag number defined in HAL will be forced fit into
    // signed int32 when passing through mojo. So all number > 0x7FFFFFFF
    // require another conversion.
    if (tag > 0x7FFFFFFF) {
      tag = /** @type {CameraMetadataTag} */ (-(~tag + 1));
    }
    const device = await this.getDevice_(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    const staticMetadata = cameraInfo.staticCameraCharacteristics;
    return getMetadataData(staticMetadata, tag);
  }

  /**
   * Gets vid:pid identifier of USB camera.
   * @param {string} deviceId
   * @return {!Promise<?string>} Identifier formatted as "vid:pid" or null for
   *     non-USB camera.
   */
  async getVidPid(deviceId) {
    const getTag = async (tag) => {
      const data = await this.getStaticMetadata(deviceId, tag);
      if (data.length === 0) {
        return null;
      }
      // Check and pop the \u0000 c style string terminal symbol.
      if (data[data.length - 1] === 0) {
        data.pop();
      }
      return String.fromCharCode(...data);
    };
    const vid = await getTag(0x80010000);
    const pid = await getTag(0x80010001);
    return vid && pid && `${vid}:${pid}`;
  }

  /**
   * Gets supported photo resolutions for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!ResolutionList>} Promise of supported resolutions.
   * @throws {!Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getPhotoResolutions(deviceId) {
    const formatBlob = 33;
    const typeOutputStream = 0;
    const numElementPerEntry = 4;

    const streamConfigs = await this.getStaticMetadata(
        deviceId,
        CameraMetadataTag.ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
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
   * @return {!Promise<!Array<!VideoConfig>>} Promise of supported video
   *     configurations.
   * @throws {!Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getVideoConfigs(deviceId) {
    // Currently we use YUV format for both recording and previewing on Chrome.
    const formatYuv = 35;
    const oneSecondInNs = 1e9;
    const numElementPerEntry = 4;

    const minFrameDurationConfigs = await this.getStaticMetadata(
        deviceId,
        CameraMetadataTag.ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
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
   * @return {!Promise<!Facing>} Promise of device facing.
   * @throws {!Error} Thrown when the device operation is not supported.
   */
  async getCameraFacing(deviceId) {
    const device = await this.getDevice_(deviceId);
    const {cameraInfo: {facing}} = await device.getCameraInfo();
    switch (facing) {
      case CameraFacing.CAMERA_FACING_BACK:
        return Facing.ENVIRONMENT;
      case CameraFacing.CAMERA_FACING_FRONT:
        return Facing.USER;
      case CameraFacing.CAMERA_FACING_EXTERNAL:
        return Facing.EXTERNAL;
      case CameraFacing.CAMERA_FACING_VIRTUAL_BACK:
        return Facing.VIRTUAL_ENV;
      case CameraFacing.CAMERA_FACING_VIRTUAL_FRONT:
        return Facing.VIRTUAL_USER;
      case CameraFacing.CAMERA_FACING_VIRTUAL_EXTERNAL:
        return Facing.VIRTUAL_EXT;
      default:
        assertNotReached(`Unexpected facing value: ${facing}`);
    }
  }

  /**
   * Gets supported fps ranges for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!FpsRangeList>} Promise of supported fps ranges.
   *     Each range is represented as [min, max].
   * @throws {!Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getSupportedFpsRanges(deviceId) {
    const numElementPerEntry = 2;

    const availableFpsRanges = await this.getStaticMetadata(
        deviceId,
        CameraMetadataTag.ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
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
   * Gets the active array size for given device.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!Resolution>} Promise of the active array size.
   * @throws {!Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getActiveArraySize(deviceId) {
    const activeArray = await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    assert(activeArray.length === 4);
    const width = activeArray[2] - activeArray[0];
    const height = activeArray[3] - activeArray[1];
    return new Resolution(width, height);
  }

  /**
   * Gets the sensor orientation for given device.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<number>} Promise of the sensor orientation.
   * @throws {!Error} Thrown when fail to parse the metadata or the device
   *     operation is not supported.
   */
  async getSensorOrientation(deviceId) {
    const sensorOrientation = await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_SENSOR_ORIENTATION);
    assert(sensorOrientation.length === 1);
    return sensorOrientation[0];
  }

  /**
   * @param {string} deviceId
   * @return {!Promise<number|undefined>} Resolves to undefined when called with
   *     |deviceId| which don't support pan control.
   */
  async getPanDefault(deviceId) {
    const tag = /** @type{!CameraMetadataTag} */ (0x8001000d);
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @param {string} deviceId
   * @return {!Promise<number|undefined>} Resolves to undefined when called with
   *     |deviceId| which don't support tilt control.
   */
  async getTiltDefault(deviceId) {
    const tag = /** @type{!CameraMetadataTag} */ (0x80010016);
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @param {string} deviceId
   * @return {!Promise<number|undefined>} Resolves to undefined when called with
   *     |deviceId| which don't support zoom control.
   */
  async getZoomDefault(deviceId) {
    const tag = /** @type{!CameraMetadataTag} */ (0x80010019);
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * Sets the frame rate range in VCD. If the range is invalid (e.g. 0 fps), VCD
   * will fallback to use the default one.
   * @param {string} deviceId
   * @param {number} min
   * @param {number} max
   * @return {!Promise}
   */
  async setFpsRange(deviceId, min, max) {
    const hasSpecifiedFrameRateRange = min > 0 && max > 0;
    const device = await this.getDevice_(deviceId);
    const {isSuccess} = await device.setFpsRange({start: min, end: max});
    if (!isSuccess && hasSpecifiedFrameRateRange) {
      reportError(
          ErrorType.SET_FPS_RANGE_FAILURE, ErrorLevel.ERROR,
          new Error('Failed to negotiate the frame rate range.'));
    }
  }

  /**
   * @param {string} deviceId
   * @param {!Resolution} resolution
   * @return {!Promise}
   */
  async setStillCaptureResolution(deviceId, resolution) {
    const device = await this.getDevice_(deviceId);
    await device.setStillCaptureResolution(resolution);
  }

  /**
   * Sets the intent for the upcoming capture session.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @param {!CaptureIntent} captureIntent The purpose of this
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
   * @throws {!Error} Thrown when the device operation is not supported.
   */
  async isPortraitModeSupported(deviceId) {
    // TODO(wtlee): Change to portrait mode tag.
    const portraitModeTag =
        /** @type{!CameraMetadataTag} */ (0x80000000);

    const portraitMode =
        await this.getStaticMetadata(deviceId, portraitModeTag);
    return portraitMode.length > 0;
  }

  /**
   * Adds a metadata observer to Camera App Device through Mojo IPC.
   * @param {string} deviceId The id for target camera device.
   * @param {function(!CameraMetadata): void} callback Callback that
   *     handles the metadata.
   * @param {!StreamType} streamType Stream type which the observer
   *     gets the metadata from.
   * @return {!Promise<!MojoEndpoint>} Added observer endpoint.
   * @throws {!Error} if fails to construct device connection.
   */
  async addMetadataObserver(deviceId, callback, streamType) {
    const observerCallbackRouter =
        wrapEndpoint(new ResultMetadataObserverCallbackRouter());
    observerCallbackRouter.onMetadataAvailable.addListener(callback);

    const device = await this.getDevice_(deviceId);
    await device.addResultMetadataObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote(), streamType);
    return observerCallbackRouter;
  }

  /**
   * Adds observer to observe shutter event.
   *
   * The shutter event is defined as CAMERA3_MSG_SHUTTER in
   * media/capture/video/chromeos/mojom/camera3.mojom which will be sent from
   * underlying camera HAL after sensor finishes frame capturing.
   *
   * @param {string} deviceId The id for target camera device.
   * @param {function(): void} callback Callback to trigger on shutter done.
   * @return {!Promise<!MojoEndpoint>} Added observer endpoint.
   * @throws {!Error} if fails to construct device connection.
   */
  async addShutterObserver(deviceId, callback) {
    const observerCallbackRouter =
        wrapEndpoint(new CameraEventObserverCallbackRouter());
    observerCallbackRouter.onShutterDone.addListener(callback);

    const device = await this.getDevice_(deviceId);
    await device.addCameraEventObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    return observerCallbackRouter;
  }

  /**
   * Sets reprocess options which are normally effects to the video capture
   * device before taking picture.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @param {!Array<!Effect>} effects The target reprocess options
   *     (effects) that would be applied on the result.
   * @return {!Promise<!Array<Promise<!Blob>>>} Array of captured results with
   *     given effect.
   * @throws {!Error} Thrown when the reprocess is failed or the device
   *     operation is not supported.
   */
  async setReprocessOptions(deviceId, effects) {
    const reprocessEvents = new Map;
    const callbacks = [];
    for (const effect of effects) {
      // TODO(pihsun): This should be WaitableEvent<Blob|Error>, since we
      // sometimes call event.signal with Error.
      /** @type {WaitableEvent<Blob>} */
      const event = new WaitableEvent();
      reprocessEvents.set(effect, event);
      callbacks.push(event.wait());
    }

    const listenerCallbacksRouter =
        wrapEndpoint(new ReprocessResultListenerCallbackRouter());
    listenerCallbacksRouter.onReprocessDone.addListener(
        (effect, status, blob) => {
          const event = reprocessEvents.get(effect);
          if (event === undefined) {
            throw new Error(`Reprocess done with unexpected effect: ${effect}`);
          }
          if (blob === null || status !== 0) {
            event.signal(new Error(`Set reprocess failed: ${status}`));
          } else {
            const {data, mimeType} = blob;
            event.signal(new Blob([new Uint8Array(data)], {type: mimeType}));
          }
        });
    const device = await this.getDevice_(deviceId);
    await device.setReprocessOptions(
        effects, listenerCallbacksRouter.$.bindNewPipeAndPassRemote());

    Promise.allSettled(callbacks).then(() => {
      closeEndpoint(listenerCallbacksRouter);
    });
    return callbacks;
  }

  /**
   * Changes whether the camera frame rotation is enabled inside the Chrome OS
   * video capture device.
   * @param {string} deviceId The id of target camera device.
   * @param {boolean} isEnabled Whether to enable the camera frame rotation at
   *     source.
   * @return {!Promise<boolean>} Whether the operation was successful.
   */
  async setCameraFrameRotationEnabledAtSource(deviceId, isEnabled) {
    const device = await this.getDevice_(deviceId);
    const {isSuccess} =
        await device.setCameraFrameRotationEnabledAtSource(isEnabled);
    return isSuccess;
  }

  /**
   * Gets the clock-wise rotation applied on the raw camera frame in order to
   * display the camera preview upright in the UI.
   * @param {string} deviceId The id of target camera device.
   * @return {!Promise<number>} The camera frame rotation.
   */
  async getCameraFrameRotation(deviceId) {
    const device = await this.getDevice_(deviceId);
    const {rotation} = await device.getCameraFrameRotation();
    return rotation;
  }

  /**
   * Drops the connection to the video capture device in Chrome.
   * @param {string} deviceId Id of the target device.
   * @return {!Promise}
   */
  async dropConnection(deviceId) {
    const device = await this.devices_.get(deviceId);
    if (device !== undefined) {
      closeEndpoint(device);
      this.devices_.delete(deviceId);
    }
  }

  /**
   * Enables/Disables multiple streams on target camera device. The extra
   * stream will be reported as virtual video device from
   * navigator.mediaDevices.enumerateDevices().
   * @param {string} deviceId The id of target camera device.
   * @param {boolean} enabled True for enabling multiple streams.
   */
  async setMultipleStreamsEnabled(deviceId, enabled) {
    if (deviceId) {
      await this.deviceProvider_.setMultipleStreamsEnabled(deviceId, enabled);
    }
  }

  /**
   * Registers a document corners observer and triggers |callback| if the
   * detected corners are updated.
   * @param {string} deviceId The id of target camera device.
   * @param {function(!Array<Point>): void} callback Callback to
   *     trigger when the detected corners are updated.
   * @return {!Promise<!MojoEndpoint>} Added observer endpoint.
   */
  async registerDocumentCornersObserver(deviceId, callback) {
    const observerCallbackRouter =
        wrapEndpoint(new DocumentCornersObserverCallbackRouter());
    observerCallbackRouter.onDocumentCornersUpdated.addListener((corners) => {
      callback(corners.map((c) => new Point(c.x, c.y)));
    });

    const device = await this.getDevice_(deviceId);
    await device.registerDocumentCornersObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    return observerCallbackRouter;
  }

  /**
   * Returns whether the blob video snapshot feature is enabled on the device.
   * @param {string} deviceId The id of target camera device.
   * @return {!Promise<boolean>}
   */
  async isBlobVideoSnapshotEnabled(deviceId) {
    const level = (await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL))[0];
    return level ===
        AndroidInfoSupportedHardwareLevel
            .ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
  }

  /**
   * Creates a new instance of DeviceOperator if it is not set. Returns the
   *     exist instance.
   * @return {!Promise<!DeviceOperator>} The singleton instance.
   */
  static async getInstance() {
    await readyEvent.wait();
    if (instance === null) {
      instance = new DeviceOperator();
    }
    if (!await instance.isSupported_) {
      return null;
    }

    // Using a wrapper to ensure all the device operations are sequentialized.
    const deviceOperatorWrapper = {
      get: function(target, property) {
        if (target[property] instanceof Function) {
          return (...args) =>
                     operationQueue.push(() => target[property](...args));
        }
        return target[property];
      },
    };
    return new Proxy(instance, deviceOperatorWrapper);
  }

  /**
   * Gets if DeviceOperator is supported.
   * @return {!Promise<boolean>} True if the DeviceOperator is supported.
   */
  static async isSupported() {
    return await this.getInstance() !== null;
  }
}
