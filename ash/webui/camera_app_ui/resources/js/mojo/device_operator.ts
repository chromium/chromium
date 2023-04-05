// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertNotReached} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import {reportError} from '../error.js';
import {Point} from '../geometry.js';
import * as state from '../state.js';
import {
  CameraSuspendError,
  ErrorLevel,
  ErrorType,
  Facing,
  FpsRangeList,
  PortraitModeProcessError,
  Resolution,
  ResolutionList,
  VideoConfig,
} from '../type.js';
import {CancelableEvent} from '../waitable_event.js';

import {
  AndroidInfoSupportedHardwareLevel,
  CameraAppDeviceProvider,
  CameraAppDeviceRemote,
  CameraEventObserverCallbackRouter,
  CameraFacing,
  CameraInfo,
  CameraInfoObserverCallbackRouter,
  CameraMetadata,
  CameraMetadataEntry,
  CameraMetadataTag,
  CaptureIntent,
  DocumentCornersObserverCallbackRouter,
  Effect,
  EntryType,
  GetCameraAppDeviceStatus,
  MojoBlob,
  PointF,
  ReprocessResultListenerCallbackRouter,
  ResultMetadataObserverCallbackRouter,
  StreamType,
} from './type.js';
import {
  closeEndpoint,
  MojoEndpoint,
  wrapEndpoint,
} from './util.js';

/**
 * Parse the entry data according to its type.
 *
 * @param entry Camera metadata entry from which to parse the data according to
 *     its type.
 * @return An array containing elements whose types correspond to the format of
 *     input |tag|.
 * @throws If entry type is not supported.
 */
export function parseMetadata(entry: CameraMetadataEntry): number[] {
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
 *
 * @param metadata Camera metadata from which to query the data.
 * @param tag Camera metadata tag to query for.
 * @return An array containing elements whose types correspond to the format of
 *     input |tag|. If nothing is found, returns an empty array.
 */
function getMetadataData(
    metadata: CameraMetadata, tag: CameraMetadataTag): number[] {
  if (metadata.entries === undefined) {
    return [];
  }
  for (let i = 0; i < metadata.entryCount; i++) {
    const entry = metadata.entries[i];
    if (entry.tag === tag) {
      return parseMetadata(entry);
    }
  }
  return [];
}

/**
 * The singleton instance of DeviceOperator. Initialized by calling
 * initializeInstance().
 *
 * Note that undefined means not initialized, and null means not supported.
 */
let instance: DeviceOperator|null|undefined = undefined;

/**
 * Job queue to sequentialize devices operations.
 */
const operationQueue = new AsyncJobQueue();

/**
 * Operates video capture device through CrOS Camera App Mojo interface.
 */
export class DeviceOperator {
  /**
   * An interface remote that is used to construct the mojo interface.
   */
  private readonly deviceProvider =
      wrapEndpoint(CameraAppDeviceProvider.getRemote());

  /**
   * Map which maps from device id to the remote of devices. We want to have
   * only one remote for each devices to avoid unnecessary wastes of resources
   * and also makes it easier to control the connection.
   */
  private readonly devices = new Map<string, Promise<CameraAppDeviceRemote>>();

  /**
   * Map for cached camera infos.
   *
   * The relation of this and cameraInfoErrorHandlers for a particular cameraId
   * is as follows:
   *
   * -------------------------------------------------------------------------.
   *     | cameraInfos.get(deviceId)  | cameraInfoErrorHandlers.get(deviceId)
   * -------------------------------------------------------------------------.
   *     |         undefined          |           undefined
   * (1)>|                            |
   *     | pending CameraInfo Promise |    reject() of the pending promise
   * (2)>|                            |
   *     |      cached CameraInfo     |           undefined
   * (3)>|                            |
   *     |     updated CameraInfo     |           undefined
   * (4)>|                            |
   *     |         undefined          |           undefined
   * -------------------------------------------------------------------------.
   *
   * (1) The getCameraInfo() is first called, for all other calls between (1)
   *     and (2), the same pending promise will be returned and might fail if
   *     cameraInfoErrorHandlers is called.
   * (2) The first CameraInfo is returned from onCameraInfoUpdated callback.
   * (3) The updated CameraInfo is returned from onCameraInfoUpdated callback.
   *     For any getCameraInfo() calls between (2) and (4), the cached camera
   *     info is returned immediately and the call never fails.
   * (4) removeDevice() is called. The cached info are deleted after this.
   */
  private readonly cameraInfos =
      new Map<string, CameraInfo|Promise<CameraInfo>>();

  /**
   * Map for camera info error handlers.
   */
  private readonly cameraInfoErrorHandlers =
      new Map<string, (error: Error) => void>();

  /**
   * Return if the direct communication between camera app and video capture
   * devices is supported.
   */
  async isSupported(): Promise<boolean> {
    const {isSupported} = await this.deviceProvider.isSupported();
    return isSupported;
  }

  /**
   * Removes a device from the maps.
   */
  removeDevice(deviceId: string): void {
    this.devices.delete(deviceId);

    const errorHandler = this.cameraInfoErrorHandlers.get(deviceId);
    if (errorHandler !== undefined) {
      errorHandler(new Error('Camera info retrieval is canceled'));
      this.cameraInfoErrorHandlers.delete(deviceId);
    }
    this.cameraInfos.delete(deviceId);
  }

  /**
   * Gets corresponding device remote by given id.
   *
   * @param deviceId The id of target camera device.
   * @return Corresponding device remote.
   * @throws Thrown when given device id is invalid.
   */
  private getDevice(deviceId: string): Promise<CameraAppDeviceRemote> {
    const d = this.devices.get(deviceId);
    if (d !== undefined) {
      return d;
    }
    const newDevice = (async () => {
      try {
        const {device, status} =
            await this.deviceProvider.getCameraAppDevice(deviceId);
        if (status === GetCameraAppDeviceStatus.ERROR_INVALID_ID) {
          throw new Error(`Invalid device id`);
        }
        if (device === null) {
          throw new Error('Unknown error');
        }
        device.onConnectionError.addListener(() => {
          this.removeDevice(deviceId);
        });
        return wrapEndpoint(device);
      } catch (e) {
        this.removeDevice(deviceId);
        throw e;
      }
    })();
    this.devices.set(deviceId, newDevice);
    return newDevice;
  }

  private async getCameraInfo(deviceId: string): Promise<CameraInfo> {
    const info = this.cameraInfos.get(deviceId);
    if (info !== undefined) {
      return info;
    }
    const pendingInfo = this.registerCameraInfoObserver(deviceId);
    this.cameraInfos.set(deviceId, pendingInfo);
    return pendingInfo;
  }

  /**
   * Gets metadata for the given device from its static characteristics.
   *
   * @param deviceId The id of target camera device.
   * @param tag Camera metadata tag to query.
   * @return Promise of the corresponding data array.
   * @throws Thrown when given device id is invalid.
   */
  async getStaticMetadata(deviceId: string, tag: CameraMetadataTag):
      Promise<number[]> {
    // All the unsigned vendor tag number defined in HAL will be forced fit into
    // signed int32 when passing through mojo. So all number > 0x7FFFFFFF
    // require another conversion.
    if (tag > 0x7FFFFFFF) {
      tag = -(~tag + 1);
    }
    const cameraInfo = await this.getCameraInfo(deviceId);
    const staticMetadata = cameraInfo.staticCameraCharacteristics;
    return getMetadataData(staticMetadata, tag);
  }

  /**
   * Gets vid:pid identifier of USB camera.
   *
   * @return Identifier formatted as "vid:pid" or null for non-USB camera.
   */
  async getVidPid(deviceId: string): Promise<string|null> {
    const getTag = async (tag: number) => {
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
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of supported resolutions.
   * @throws Thrown when fail to parse the metadata or the device operation is
   *    not supported.
   */
  async getPhotoResolutions(deviceId: string): Promise<ResolutionList> {
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
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of supported video configurations.
   * @throws Thrown when fail to parse the metadata or the device operation is
   *     not supported.
   */
  async getVideoConfigs(deviceId: string): Promise<VideoConfig[]> {
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
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of device facing.
   * @throws Thrown when the device operation is not supported.
   */
  async getCameraFacing(deviceId: string): Promise<Facing> {
    const {facing} = await this.getCameraInfo(deviceId);
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
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of supported fps ranges.  Each range is represented as
   *     [min, max].
   * @throws Thrown when fail to parse the metadata or the device operation is
   *     not supported.
   */
  async getSupportedFpsRanges(deviceId: string): Promise<FpsRangeList> {
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

    const supportedFpsRanges: FpsRangeList = [];
    for (let i = 0; i < availableFpsRanges.length; i += numElementPerEntry) {
      const [minFps, maxFps] =
          availableFpsRanges.slice(i, i + numElementPerEntry);
      supportedFpsRanges.push({minFps, maxFps});
    }
    return supportedFpsRanges;
  }

  /**
   * Gets the active array size for given device.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of the active array size.
   * @throws Thrown when fail to parse the metadata or the device operation is
   *     not supported.
   */
  async getActiveArraySize(deviceId: string): Promise<Resolution> {
    const activeArray = await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    assert(activeArray.length === 4);
    const width = activeArray[2] - activeArray[0];
    const height = activeArray[3] - activeArray[1];
    return new Resolution(width, height);
  }

  /**
   * Gets the sensor orientation for given device.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of the sensor orientation.
   * @throws Thrown when fail to parse the metadata or the device operation is
   *     not supported.
   */
  async getSensorOrientation(deviceId: string): Promise<number> {
    const sensorOrientation = await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_SENSOR_ORIENTATION);
    assert(sensorOrientation.length === 1);
    return sensorOrientation[0];
  }

  /**
   * @return Resolves to undefined when called with |deviceId| which don't
   *     support pan control.
   */
  async getPanDefault(deviceId: string): Promise<number|undefined> {
    const tag = 0x8001000d as CameraMetadataTag;
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @return Resolves to undefined when called with |deviceId| which don't
   *     support tilt control.
   */
  async getTiltDefault(deviceId: string): Promise<number|undefined> {
    const tag = 0x80010016 as CameraMetadataTag;
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @return Resolves to undefined when called with |deviceId| which don't
   *     support zoom control.
   */
  async getZoomDefault(deviceId: string): Promise<number|undefined> {
    const tag = 0x80010019 as CameraMetadataTag;
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * Sets the frame rate range in VCD. If the range is invalid (e.g. 0 fps), VCD
   * will fallback to use the default one.
   */
  async setFpsRange(deviceId: string, min: number, max: number): Promise<void> {
    const hasSpecifiedFrameRateRange = min > 0 && max > 0;
    const device = await this.getDevice(deviceId);
    const {isSuccess} = await device.setFpsRange({start: min, end: max});
    if (!isSuccess && hasSpecifiedFrameRateRange) {
      reportError(
          ErrorType.SET_FPS_RANGE_FAILURE, ErrorLevel.ERROR,
          new Error('Failed to negotiate the frame rate range.'));
    }
  }

  async setStillCaptureResolution(deviceId: string, resolution: Resolution):
      Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.setStillCaptureResolution(resolution);
  }

  /**
   * Sets the intent for the upcoming capture session.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @param captureIntent The purpose of this capture, to help the camera
   *     device decide optimal configurations.
   * @return Promise for the operation.
   */
  async setCaptureIntent(deviceId: string, captureIntent: CaptureIntent):
      Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.setCaptureIntent(captureIntent);
  }

  /**
   * Checks if portrait mode is supported.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Promise of the boolean result.
   * @throws Thrown when the device operation is not supported.
   */
  async isPortraitModeSupported(deviceId: string): Promise<boolean> {
    // TODO(wtlee): Change to portrait mode tag.
    const portraitModeTag = 0x80000000 as CameraMetadataTag;

    const portraitMode =
        await this.getStaticMetadata(deviceId, portraitModeTag);
    return portraitMode.length > 0;
  }

  /**
   * Adds a metadata observer to Camera App Device through Mojo IPC.
   *
   * @param deviceId The id for target camera device.
   * @param callback Callback that handles the metadata.
   * @param streamType Stream type which the observer gets the metadata from.
   * @return Added observer endpoint.
   * @throws If fails to construct device connection.
   */
  async addMetadataObserver(
      deviceId: string, callback: (metadata: CameraMetadata) => void,
      streamType: StreamType): Promise<MojoEndpoint> {
    const observerCallbackRouter =
        wrapEndpoint(new ResultMetadataObserverCallbackRouter());
    observerCallbackRouter.onMetadataAvailable.addListener(callback);

    const device = await this.getDevice(deviceId);
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
   * @param deviceId The id for target camera device.
   * @param callback Callback to trigger on shutter done.
   * @return Added observer endpoint.
   * @throws If fails to construct device connection.
   */
  async addShutterObserver(deviceId: string, callback: () => void):
      Promise<MojoEndpoint> {
    const observerCallbackRouter =
        wrapEndpoint(new CameraEventObserverCallbackRouter());
    observerCallbackRouter.onShutterDone.addListener(callback);

    const device = await this.getDevice(deviceId);
    await device.addCameraEventObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    return observerCallbackRouter;
  }

  /**
   * Sets reprocess options which are normally effects to the video capture
   * device before taking picture.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @param effects The target reprocess options (effects) that would be
   *     applied on the result.
   * @return Array of captured results with given effect.
   * @throws Thrown when the reprocess is failed or the device operation is not
   *     supported.
   */
  async setReprocessOptions(deviceId: string, effects: Effect[]):
      Promise<Array<Promise<Blob>>> {
    const reprocessEvents = new Map<Effect, CancelableEvent<Blob>>();
    const callbacks = [];
    for (const effect of effects) {
      const event = new CancelableEvent<Blob>();
      reprocessEvents.set(effect, event);
      callbacks.push(event.wait());
    }

    const listenerCallbacksRouter =
        wrapEndpoint(new ReprocessResultListenerCallbackRouter());
    listenerCallbacksRouter.onReprocessDone.addListener(
        (effect: Effect, status: number, blob: MojoBlob|null) => {
          const event = assertExists(reprocessEvents.get(effect));
          // The definitions of status code is not exposed to Chrome so we are
          // not able to distinguish between different kinds of errors.
          // TODO(b/220056961): Handle errors respectively once we have the
          // definitions.
          // Ref:
          // https://source.corp.google.com/chromeos_public/src/platform2/camera/hal_adapter/reprocess_effect/portrait_mode_effect.h;rcl=dd67a0b4be973da51324be2ff2dd243125e27f07;l=77
          if (effect === Effect.PORTRAIT_MODE && status !== 0) {
            event.signalError(new PortraitModeProcessError());
          } else if (blob === null || status !== 0) {
            event.signalError(new Error(`Set reprocess failed: ${status}`));
          } else {
            const {data, mimeType} = blob;
            event.signal(new Blob([new Uint8Array(data)], {type: mimeType}));
          }
        });

    function suspendObserver(val: boolean) {
      if (val) {
        console.warn('camera suspended');
        for (const [effect, event] of reprocessEvents.entries()) {
          if (effect === Effect.PORTRAIT_MODE) {
            event.signalError(new CameraSuspendError());
          }
        }
      }
    }
    state.addOneTimeObserver(state.State.SUSPEND, suspendObserver);

    const device = await this.getDevice(deviceId);
    await device.setReprocessOptions(
        effects, listenerCallbacksRouter.$.bindNewPipeAndPassRemote());

    Promise.allSettled(callbacks).then(() => {
      state.removeObserver(state.State.SUSPEND, suspendObserver);
      closeEndpoint(listenerCallbacksRouter);
    });
    return callbacks;
  }

  /**
   * Changes whether the camera frame rotation is enabled inside the ChromeOS
   * video capture device.
   *
   * @param deviceId The id of target camera device.
   * @param isEnabled Whether to enable the camera frame rotation at source.
   * @return Whether the operation was successful.
   */
  async setCameraFrameRotationEnabledAtSource(
      deviceId: string, isEnabled: boolean): Promise<boolean> {
    const device = await this.getDevice(deviceId);
    const {isSuccess} =
        await device.setCameraFrameRotationEnabledAtSource(isEnabled);
    return isSuccess;
  }

  /**
   * Gets the clock-wise rotation applied on the raw camera frame in order to
   * display the camera preview upright in the UI.
   *
   * @param deviceId The id of target camera device.
   * @return The camera frame rotation.
   */
  async getCameraFrameRotation(deviceId: string): Promise<number> {
    const device = await this.getDevice(deviceId);
    const {rotation} = await device.getCameraFrameRotation();
    return rotation;
  }

  /**
   * Drops the connection to the video capture device in Chrome.
   *
   * @param deviceId Id of the target device.
   */
  async dropConnection(deviceId: string): Promise<void> {
    const device = await this.devices.get(deviceId);
    if (device !== undefined) {
      closeEndpoint(device);
      this.removeDevice(deviceId);
    }
  }

  /**
   * Enables/Disables virtual device on target camera device. The extra
   * stream will be reported as virtual video device from
   * navigator.mediaDevices.enumerateDevices().
   *
   * @param deviceId The id of target camera device.
   * @param enabled True for enabling virtual device.
   */
  async setVirtualDeviceEnabled(deviceId: string, enabled: boolean):
      Promise<void> {
    if (deviceId) {
      await this.deviceProvider.setVirtualDeviceEnabled(deviceId, enabled);
    }
  }

  /**
   * Enable/Disables the multiple streams feature for video recording on the
   * target camera device.
   */
  async setMultipleStreamsEnabled(deviceId: string, enabled: boolean):
      Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.setMultipleStreamsEnabled(enabled);
  }

  /**
   * Registers a document corners observer and triggers |callback| if the
   * detected corners are updated.
   *
   * @param deviceId The id of target camera device.
   * @param callback Callback to trigger when the detected corners are updated.
   * @return Added observer endpoint.
   */
  async registerDocumentCornersObserver(
      deviceId: string,
      callback: (corners: Point[]) => void): Promise<MojoEndpoint> {
    const observerCallbackRouter =
        wrapEndpoint(new DocumentCornersObserverCallbackRouter());
    observerCallbackRouter.onDocumentCornersUpdated.addListener(
        (corners: PointF[]) => {
          callback(corners.map((c) => new Point(c.x, c.y)));
        });

    const device = await this.getDevice(deviceId);
    await device.registerDocumentCornersObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    return observerCallbackRouter;
  }

  /**
   * Registers observer to monitor when the camera info is updated.
   *
   * @param deviceId The id of the target camera device.
   * @return The initial camera info.
   */
  async registerCameraInfoObserver(deviceId: string): Promise<CameraInfo> {
    const observerCallbackRouter =
        wrapEndpoint(new CameraInfoObserverCallbackRouter());
    observerCallbackRouter.onCameraInfoUpdated.addListener(
        (info: CameraInfo) => {
          this.cameraInfos.set(deviceId, info);
          onInfoReady.signal(info);
        });
    const device = await this.getDevice(deviceId);

    const onInfoReady = new CancelableEvent<CameraInfo>();
    // Note that this needs to be set after this.getDevice, otherwise, the
    // getDevice() might raise an exception, but the onInfoReady.signalError()
    // will also be called in removeDevice, which cause an unhandled promise
    // rejection since onInfoReady.wait() won't be called in this case.
    this.cameraInfoErrorHandlers.set(deviceId, (e) => {
      onInfoReady.signalError(e);
    });
    await device.registerCameraInfoObserver(
        observerCallbackRouter.$.bindNewPipeAndPassRemote());
    try {
      return await onInfoReady.wait();
    } finally {
      this.cameraInfoErrorHandlers.delete(deviceId);
    }
  }

  /**
   * Returns whether the blob video snapshot feature is enabled on the device.
   *
   * @param deviceId The id of target camera device.
   */
  async isBlobVideoSnapshotEnabled(deviceId: string): Promise<boolean> {
    const level = (await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL))[0];
    const supportedLevel = [
      AndroidInfoSupportedHardwareLevel
          .ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL,
      AndroidInfoSupportedHardwareLevel.ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_3,
    ];
    return supportedLevel.includes(level);
  }

  /**
   * Initialize the singleton instance.
   *
   * This should be called before all invocation of static getInstance() and
   * static isSupported().
   */
  static async initializeInstance(): Promise<void> {
    assert(instance === undefined);
    const rawInstance = new DeviceOperator();
    if (!await rawInstance.isSupported()) {
      instance = null;
      return;
    }

    // Using a wrapper to ensure all the device operations are sequentialized.
    const deviceOperatorWrapper: ProxyHandler<DeviceOperator> = {
      get: function(target, property) {
        const val = Reflect.get(target, property);
        if (val instanceof Function) {
          return (...args: unknown[]) => operationQueue.push(
                     () =>
                         Reflect.apply(val, target, args) as Promise<unknown>);
        }
        return val;
      },
    };
    instance = new Proxy(rawInstance, deviceOperatorWrapper);
  }

  /**
   * Returns the existing singleton instance of DeviceOperator.
   *
   * @return The singleton instance.
   */
  static getInstance(): DeviceOperator|null {
    assert(instance !== undefined);
    return instance;
  }

  /**
   * Gets if DeviceOperator is supported.
   *
   * @return True if the DeviceOperator is supported.
   */
  static isSupported(): boolean {
    return this.getInstance() !== null;
  }
}
