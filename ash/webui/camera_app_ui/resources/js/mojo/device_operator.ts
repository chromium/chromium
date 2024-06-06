// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertNotReached} from '../assert.js';
import {AsyncJobWithResultQueue} from '../async_job_queue.js';
import {reportError} from '../error.js';
import {Point} from '../geometry.js';
import {isLocalDev} from '../models/load_time_data.js';
import * as state from '../state.js';
import {
  CameraSuspendError,
  CropRegionRect,
  ErrorLevel,
  ErrorType,
  Facing,
  FpsRangeList,
  PortraitErrorNoFaceDetected,
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
  PortraitModeSegResult,
  ResultMetadataObserverCallbackRouter,
  StillCaptureResultObserverCallbackRouter,
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
 * @return An array containing elements whose types correspond to the format of
 *     input |entry|.
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
 * Gets the data from Camera metadata by given |tag|.
 *
 * @return An array containing elements whose types correspond to the format of
 *     input |tag|. If nothing is found, returns an empty array.
 */
function getMetadataData(
    metadata: CameraMetadata, tag: CameraMetadataTag): number[] {
  if (metadata.entries === undefined) {
    return [];
  }
  for (let i = 0; i < metadata.entryCount; i++) {
    // Disabling check because this code assumes that metadata.entries is
    // either undefined or defined, but at runtime Mojo will always set this
    // to null or defined.
    // TODO(crbug.com/40267104): If this function only handles data
    // from Mojo, the assertion above should be changed to null and the
    // null error suppression can be removed.
    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
    const entry = metadata.entries![i];
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
const operationQueue = new AsyncJobWithResultQueue();

/**
 * Operates video capture device through CrOS Camera App Mojo interface.
 */
export class DeviceOperator {
  /**
   * An interface remote that is used to construct the mojo interface.
   */
  private readonly deviceProvider =
      isLocalDev() ? null : wrapEndpoint(CameraAppDeviceProvider.getRemote());

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
   * Map which maps from device id to camera info error handlers.
   */
  private readonly cameraInfoErrorHandlers =
      new Map<string, (error: Error) => void>();

  /**
   * Returns if the direct communication between camera app and video capture
   * devices is supported.
   */
  async isSupported(): Promise<boolean> {
    if (this.deviceProvider === null) {
      return false;
    }
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
   * Check if the device is in use.
   *
   * @param deviceId The id of target camera device.
   */
  async isDeviceInUse(deviceId: string): Promise<boolean> {
    assert(this.deviceProvider !== null);
    const {inUse} = await this.deviceProvider.isDeviceInUse(deviceId);
    return inUse;
  }

  /**
   * Gets corresponding device remote by given |deviceId|.
   *
   * @throws Thrown when given |deviceId| is invalid.
   */
  private getDevice(deviceId: string): Promise<CameraAppDeviceRemote> {
    const d = this.devices.get(deviceId);
    if (d !== undefined) {
      return d;
    }
    const newDevice = (async () => {
      try {
        assert(this.deviceProvider !== null);
        const {device, status} =
            await this.deviceProvider.getCameraAppDevice(deviceId);
        if (status === GetCameraAppDeviceStatus.kErrorInvalidId) {
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
    if (vid === null || pid === null) {
      return null;
    }
    return `${vid}:${pid}`;
  }

  /**
   * Gets supported photo resolutions for specific camera.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
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
    // This is a custom USB HAL vendor tag, defined in hal/usb/vendor_tag.h on
    // platform side.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    const tag = 0x8001000d as CameraMetadataTag;
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @return Resolves to undefined when called with |deviceId| which don't
   *     support tilt control.
   */
  async getTiltDefault(deviceId: string): Promise<number|undefined> {
    // This is a custom USB HAL vendor tag, defined in hal/usb/vendor_tag.h on
    // platform side.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    const tag = 0x80010016 as CameraMetadataTag;
    const data = await this.getStaticMetadata(deviceId, tag);
    return data[0];
  }

  /**
   * @return Resolves to undefined when called with |deviceId| which don't
   *     support zoom control.
   */
  async getZoomDefault(deviceId: string): Promise<number|undefined> {
    // This is a custom USB HAL vendor tag, defined in hal/usb/vendor_tag.h on
    // platform side.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
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
   */
  async setCaptureIntent(deviceId: string, captureIntent: CaptureIntent):
      Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.setCaptureIntent(captureIntent);
  }

  /**
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @throws Thrown when the device operation is not supported.
   */
  async isPortraitModeSupported(deviceId: string): Promise<boolean> {
    // This is a custom vendor tag, defined in common/vendor_tag_manager.h on
    // platform side.
    // TODO(wtlee): Change to portrait mode tag.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    const portraitModeTag = 0x80000000 as CameraMetadataTag;

    const portraitMode =
        await this.getStaticMetadata(deviceId, portraitModeTag);
    return portraitMode.length > 0;
  }

  /**
   * Adds a metadata observer to Camera App Device through Mojo IPC and returns
   * added observer endpoint.
   *
   * @param deviceId The id for target camera device.
   * @param callback Callback that handles the metadata.
   * @param streamType Stream type which the observer gets the metadata from.
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
   * Adds observer to observe shutter event and returns added observer endpoint.
   *
   * The shutter event is defined as CAMERA3_MSG_SHUTTER in
   * media/capture/video/chromeos/mojom/camera3.mojom which will be sent from
   * underlying camera HAL after sensor finishes frame capturing.
   *
   * @param deviceId The id for target camera device.
   * @param callback Callback to trigger on shutter done.
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
   * Takes portrait mode photos.
   *
   * @param deviceId The renderer-facing device id of the target camera which
   *     could be retrieved from MediaDeviceInfo.deviceId.
   * @return Array of captured results for portrait mode. The first result is
   *     the reference photo, and the second result is the photo with the bokeh
   *     effect applied.
   * @throws Thrown when the portrait mode processing is failed or the device
   *     operation is not supported.
   */
  async takePortraitModePhoto(deviceId: string): Promise<Array<Promise<Blob>>> {
    const normalCapture = new CancelableEvent<Blob>();
    const portraitCapture = new CancelableEvent<Blob>();
    const portraitEvents = new Map([
      [Effect.kNoEffect, normalCapture],
      [Effect.kPortraitMode, portraitCapture],
    ]);
    const callbacks = [normalCapture.wait(), portraitCapture.wait()];

    const listenerCallbacksRouter =
        wrapEndpoint(new StillCaptureResultObserverCallbackRouter());
    listenerCallbacksRouter.onStillCaptureDone.addListener(
        (effect: Effect, status: number, blob: MojoBlob|null) => {
          const event = assertExists(portraitEvents.get(effect));
          if (blob === null) {
            event.signalError(new Error(`Capture failed.`));
            return;
          }
          if (effect === Effect.kPortraitMode &&
              status !== PortraitModeSegResult.kSuccess) {
            // We only appends the blob result to the output when the status
            // code is `kSuccess`. For any other status code, the blob
            // will be the original photo and will not be shown to the user.
            if (status === PortraitModeSegResult.kNoFaces) {
              event.signalError(new PortraitErrorNoFaceDetected());
              return;
            }
            event.signalError(
                new Error(`Portrait processing failed: ${status}`));
            return;
          }
          const {data, mimeType} = blob;
          event.signal(new Blob([new Uint8Array(data)], {type: mimeType}));
        });

    function suspendObserver(val: boolean) {
      if (val) {
        console.warn('camera suspended');
        for (const event of portraitEvents.values()) {
          event.signalError(new CameraSuspendError());
        }
      }
    }
    state.addObserver(state.State.SUSPEND, suspendObserver);

    const device = await this.getDevice(deviceId);
    await device.takePortraitModePhoto(
        listenerCallbacksRouter.$.bindNewPipeAndPassRemote());

    // This is for cleanup after all effects are settled.
    void Promise.allSettled(callbacks).then(() => {
      state.removeObserver(state.State.SUSPEND, suspendObserver);
      closeEndpoint(listenerCallbacksRouter);
    });
    return callbacks;
  }

  /**
   * Sets whether the camera frame rotation is enabled inside the ChromeOS
   * video capture device.
   *
   * @param deviceId The id of target camera device.
   * @param enabled Whether to enable the camera frame rotation at source.
   * @return Whether the operation was successful.
   */
  async setCameraFrameRotationEnabledAtSource(
      deviceId: string, enabled: boolean): Promise<boolean> {
    const device = await this.getDevice(deviceId);
    const {isSuccess} =
        await device.setCameraFrameRotationEnabledAtSource(enabled);
    return isSuccess;
  }

  /**
   * Gets the clock-wise rotation applied on the raw camera frame in order to
   * display the camera preview upright in the UI.
   *
   * @param deviceId The id of target camera device.
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
   * Sets the crop region for the configured stream on camera with |deviceId|.
   */
  async setCropRegion(deviceId: string, cropRegion: CropRegionRect):
      Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.setCropRegion(cropRegion);
  }

  /**
   * Resets the crop region for the camera with |deviceId| to let the camera
   * stream back to full frame.
   */
  async resetCropRegion(deviceId: string): Promise<void> {
    const device = await this.getDevice(deviceId);
    await device.resetCropRegion();
  }

  /**
   * Returns whether digital zoom is supported in the camera.
   */
  async isDigitalZoomSupported(deviceId: string): Promise<boolean> {
    // Checks if the device can do zoom through the stream manipulator.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    const digitalZoomTag = 0x80070000 as CameraMetadataTag;
    const digitalZoomData =
        await this.getStaticMetadata(deviceId, digitalZoomTag);

    // Some devices can do zoom given the crop region in their HALs. This
    // ability can be checked with AVAILABLE_MAX_DIGITAL_ZOOM value being
    // greater than 1.
    const maxZoomRatio = await this.getStaticMetadata(
        deviceId, CameraMetadataTag.ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
    const hasInternalZoom = maxZoomRatio.length > 0 && maxZoomRatio[0] > 1;

    return digitalZoomData.length > 0 || hasInternalZoom;
  }

  /**
   * Initializes the singleton instance.
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
        if (!(val instanceof Function)) {
          return val;
        }
        return (...args: unknown[]) =>
                   operationQueue.push(() => Reflect.apply(val, target, args));
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
   * Returns if DeviceOperator is supported.
   */
  static isSupported(): boolean {
    return this.getInstance() !== null;
  }
}
