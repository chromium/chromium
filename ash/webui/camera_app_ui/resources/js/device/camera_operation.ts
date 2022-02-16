// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
  assertString,
} from '../assert.js';
import * as error from '../error.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  Resolution,
} from '../type.js';
import * as util from '../util.js';
import {CancelableEvent, WaitableEvent} from '../waitable_event.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  PhotoConstraintsPreferrer,
  VideoConstraintsPreferrer,
} from './constraints_preferrer.js';
import {DeviceInfoUpdater} from './device_info_updater.js';
import {Modes, Video} from './mode/index.js';
import {Preview} from './preview.js';
import {StreamConstraints} from './stream_constraints.js';
import {StreamManager} from './stream_manager.js';
import {
  CameraConfig,
  CameraInfo,
  CameraViewUI,
  ModeConstraints,
} from './type.js';


interface ConfigureCandidate {
  deviceId: string;
  mode: Mode;
  captureResolution: Resolution;
  constraints: StreamConstraints;
  videoSnapshotResolution: Resolution;
}

export interface EventListener {
  onTryingNewConfig(config: CameraConfig): void;
  onUpdateConfig(config: CameraConfig): Promise<void>;
  onUpdateCapability(cameraInfo: CameraInfo): void;
}

/**
 * Controller for closing or opening camera with specific |CameraConfig|.
 */
class Reconfigurer {
  /**
   * Preferred configuration.
   */
  public config: CameraConfig;

  private shouldSuspend = false;

  constructor(
      private readonly preview: Preview,
      private readonly modes: Modes,
      private readonly listener: EventListener,
      private readonly modeConstraints: ModeConstraints,
      facing: Facing,
  ) {
    const mode = util.assertEnumVariant(
        Mode, this.modeConstraints.exact ?? this.modeConstraints.default);
    this.config = {deviceId: null, facing, mode};
  }

  setShouldSuspend(value: boolean) {
    this.shouldSuspend = value;
  }

  /**
   * Gets the video device ids sorted by preference.
   */
  private getDeviceIdCandidates(cameraInfo: CameraInfo): string[] {
    let devices: Array<Camera3DeviceInfo|MediaDeviceInfo>;
    /**
     * Object mapping from device id to facing. Set to null for fake cameras.
     */
    let facings: Record<string, Facing>|null = null;

    const camera3Info = cameraInfo.camera3DevicesInfo;
    if (camera3Info !== null) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
    } else {
      devices = cameraInfo.devicesInfo;
    }

    const preferredFacing = this.config.facing === Facing.NOT_SET ?
        util.getDefaultFacing() :
        this.config.facing;
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.config.deviceId !== null ?
              a === this.config.deviceId :
              (facings && facings[a] === preferredFacing)) {
        return -1;
      }
      return 1;
    });
    return sorted;
  }

  private async getModeCandidates(deviceId: string|null): Promise<Mode[]> {
    if (this.modeConstraints.exact !== undefined) {
      assert(
          await this.modes.isSupported(this.modeConstraints.exact, deviceId));
      return [this.modeConstraints.exact];
    }
    return this.modes.getModeCandidates(deviceId, this.config.mode);
  }

  private async *
      getConfigurationCandidates(cameraInfo: CameraInfo):
          AsyncIterable<ConfigureCandidate> {
    const deviceOperator = await DeviceOperator.getInstance();

    for (const deviceId of this.getDeviceIdCandidates(cameraInfo)) {
      for (const mode of await this.getModeCandidates(deviceId)) {
        let resolCandidates;
        let photoRs;
        if (deviceOperator !== null) {
          resolCandidates = this.modes.getResolutionCandidates(mode, deviceId);
          photoRs = await deviceOperator.getPhotoResolutions(deviceId);
        } else {
          resolCandidates =
              this.modes.getFakeResolutionCandidates(mode, deviceId);
          photoRs = resolCandidates.map((c) => c.resolution);
        }
        const maxResolution =
            photoRs.reduce((maxR, r) => r.area > maxR.area ? r : maxR);
        for (const {
               resolution: captureResolution,
               previewCandidates,
             } of resolCandidates) {
          const videoSnapshotResolution =
              state.get(state.State.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT) ?
              maxResolution :
              captureResolution;
          for (const constraints of previewCandidates) {
            yield {
              deviceId,
              mode,
              captureResolution,
              constraints,
              videoSnapshotResolution,
            };
          }
        }
      }
    }
  }

  /**
   * Checks if PTZ can be enabled.
   */
  private async checkEnablePTZ(c: ConfigureCandidate): Promise<void> {
    const enablePTZ = await (async () => {
      if (!this.preview.isSupportPTZ()) {
        return false;
      }
      const modeSupport = state.get(state.State.USE_FAKE_CAMERA) ||
          this.modes.isSupportPTZ(
              c.mode,
              c.captureResolution,
              this.preview.getResolution(),
          );
      if (!modeSupport) {
        await this.preview.resetPTZ();
        return false;
      }
      return true;
    })();
    state.set(state.State.ENABLE_PTZ, enablePTZ);
  }

  async start(cameraInfo: CameraInfo): Promise<boolean> {
    await this.stopStreams();
    return this.startConfigure(cameraInfo);
  }

  /**
   * @return If the reconfiguration finished successfully.
   */
  async startConfigure(cameraInfo: CameraInfo): Promise<boolean> {
    if (this.shouldSuspend) {
      return false;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);

    for await (const c of this.getConfigurationCandidates(cameraInfo)) {
      if (this.shouldSuspend) {
        return false;
      }

      const nextConfig: CameraConfig = {
        deviceId: c.deviceId,
        facing: (c.deviceId &&
                 cameraInfo.getCamera3DeviceInfo(c.deviceId)?.facing) ??
            Facing.NOT_SET,
        mode: c.mode,
      };
      this.listener.onTryingNewConfig(nextConfig);
      this.modes.setCaptureParams(
          c.mode, c.constraints, c.captureResolution,
          c.videoSnapshotResolution);
      try {
        await this.modes.prepareDevice();
        const factory = this.modes.getModeFactory(c.mode);
        const stream = await this.preview.open(c.constraints);
        // For legacy linux VCD, the facing and device id can only be known
        // after preview is actually opened.
        const facing = this.preview.getFacing();
        nextConfig.facing = facing;
        const deviceId = assertString(this.preview.getDeviceId());
        nextConfig.deviceId = deviceId;

        await this.checkEnablePTZ(c);
        factory.setPreviewVideo(this.preview.getVideo());
        factory.setFacing(facing);
        await this.modes.updateMode(factory, stream, facing, deviceId);
        this.config = nextConfig;
        await this.listener.onUpdateConfig(this.config);

        return true;
      } catch (e) {
        await this.stopStreams();

        let errorToReport = e;
        // Since OverconstrainedError is not an Error instance.
        if (e instanceof OverconstrainedError) {
          errorToReport =
              new Error(`${e.message} (constraint = ${e.constraint})`);
          errorToReport.name = 'OverconstrainedError';
        } else if (e.name === 'NotReadableError') {
          // TODO(b/187879603): Remove this hacked once we understand more
          // about such error.
          // We cannot get the camera facing from stream since it might
          // not be successfully opened. Therefore, we asked the camera
          // facing via Mojo API.
          let facing = Facing.NOT_SET;
          if (deviceOperator !== null) {
            facing = await deviceOperator.getCameraFacing(c.deviceId);
          }
          errorToReport = new Error(`${e.message} (facing = ${facing})`);
          errorToReport.name = 'NotReadableError';
        }
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(errorToReport, Error));
      }
    }
    return false;
  }

  /**
   * Stop extra stream and preview stream.
   */
  private async stopStreams() {
    await this.modes.clear();
    await this.preview.close();
  }
}

class Capturer {
  constructor(private readonly modes: Modes) {}

  async start(): Promise<() => Promise<void>> {
    return this.modes.current.startCapture();
  }

  stop() {
    this.modes.current.stopCapture();
  }

  takeVideoSnapshot() {
    if (this.modes.current instanceof Video) {
      this.modes.current.takeSnapshot();
    }
  }

  toggleVideoRecordingPause() {
    if (this.modes.current instanceof Video) {
      this.modes.current.togglePaused();
    }
  }
}

enum OperationType {
  RECONFIGURE = 'reconfigure',
  CAPTURE = 'capture',
}

export class OperationScheduler {
  public cameraInfo: CameraInfo|null = null;
  private pendingUpdateInfo: CameraInfo|null = null;
  private firstInfoUpdate = new WaitableEvent();

  readonly reconfigurer: Reconfigurer;
  readonly capturer: Capturer;
  private ongoingOperationType: OperationType|null = null;
  private pendingReconfigureWaiters: CancelableEvent<boolean>[] = [];
  public readonly photoPreferrer = new PhotoConstraintsPreferrer();
  public readonly videoPreferrer = new VideoConstraintsPreferrer();
  public readonly modes: Modes;

  constructor(
      private readonly infoUpdater: DeviceInfoUpdater,
      private readonly listener: EventListener,
      preview: Preview,
      defaultFacing: Facing,
      modeConstraints: ModeConstraints,
  ) {
    this.modes = new Modes(this.photoPreferrer, this.videoPreferrer);
    this.reconfigurer = new Reconfigurer(
        preview,
        this.modes,
        listener,
        modeConstraints,
        defaultFacing,
    );
    this.capturer = new Capturer(this.modes);
    this.infoUpdater.addDeviceChangeListener(async (updater) => {
      const info = new CameraInfo(updater);
      if (this.ongoingOperationType !== null) {
        this.pendingUpdateInfo = info;
        return;
      }
      this.doUpdate(info);
    });
  }

  async initialize(cameraViewUI: CameraViewUI): Promise<void> {
    this.modes.initialize(cameraViewUI);
    await StreamManager.getInstance().deviceUpdate();
    await this.firstInfoUpdate.wait();
  }

  private doUpdate(cameraInfo: CameraInfo) {
    const isFirstUpdate = this.cameraInfo === null;
    this.cameraInfo = cameraInfo;
    if (cameraInfo.camera3DevicesInfo !== null) {
      this.photoPreferrer.updateDevicesInfo(cameraInfo.camera3DevicesInfo);
      this.videoPreferrer.updateDevicesInfo(cameraInfo.camera3DevicesInfo);
    }
    this.listener.onUpdateCapability(cameraInfo);
    if (isFirstUpdate) {
      this.firstInfoUpdate.signal();
    }
  }

  async reconfigure(): Promise<boolean> {
    if (this.ongoingOperationType !== null) {
      const event = new CancelableEvent<boolean>();
      this.pendingReconfigureWaiters.push(event);
      this.stopCapture();
      return event.wait();
    }
    return this.startReconfigure();
  }

  takeVideoSnapshot(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.takeVideoSnapshot();
    }
  }

  toggleVideoRecordingPause(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.toggleVideoRecordingPause();
    }
  }

  private clearPendingReconfigureWaiters() {
    for (const waiter of this.pendingReconfigureWaiters) {
      waiter.signal(false);
    }
    this.pendingReconfigureWaiters = [];
  }

  private finishOperation(): void {
    this.ongoingOperationType = null;

    // Check pending operations.
    if (this.pendingUpdateInfo !== null) {
      this.doUpdate(this.pendingUpdateInfo);
      this.pendingUpdateInfo = null;
    }
    if (this.pendingReconfigureWaiters.length !== 0) {
      const starting = this.startReconfigure();
      for (const waiter of this.pendingReconfigureWaiters) {
        waiter.signalAs(starting);
      }
      this.pendingReconfigureWaiters = [];
    }
  }

  async startCapture(): Promise<() => Promise<void>>|null {
    if (this.ongoingOperationType !== null) {
      return null;
    }
    this.ongoingOperationType = OperationType.CAPTURE;

    try {
      return await this.capturer.start();
    } finally {
      this.finishOperation();
    }
  }

  stopCapture(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.stop();
    }
  }

  private async startReconfigure(): Promise<boolean> {
    assert(this.ongoingOperationType === null);
    this.ongoingOperationType = OperationType.RECONFIGURE;

    const cameraInfo = assertInstanceof(this.cameraInfo, CameraInfo);
    try {
      const succeed = await this.reconfigurer.start(cameraInfo);
      if (!succeed) {
        this.clearPendingReconfigureWaiters();
      }
      return succeed;
    } catch (e) {
      this.clearPendingReconfigureWaiters();
      throw e;
    } finally {
      this.finishOperation();
    }
  }
}
