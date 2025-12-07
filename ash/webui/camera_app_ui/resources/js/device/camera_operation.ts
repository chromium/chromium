// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
  assertString,
} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import * as error from '../error.js';
import * as expert from '../expert.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as state from '../state.js';
import {
  CameraSuspendError,
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  NoCameraError,
  Resolution,
} from '../type.js';
import * as util from '../util.js';
import {CancelableEvent, WaitableEvent} from '../waitable_event.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {
  CaptureCandidate,
  FakeCameraCaptureCandidate,
} from './capture_candidate.js';
import {CaptureCandidatePreferrer} from './capture_candidate_preferrer.js';
import {DeviceMonitor} from './device_monitor.js';
import {Modes, Video} from './mode/index.js';
import {Preview} from './preview.js';
import {StreamConstraints} from './stream_constraints.js';
import {
  CameraConfig,
  CameraConfigCandidate,
  CameraInfo,
  CameraViewUi,
  ModeConstraints,
} from './type.js';


interface ConfigureCandidate {
  deviceId: string;
  mode: Mode;
  captureCandidate: CaptureCandidate;
  constraints: StreamConstraints;
  videoSnapshotResolution: Resolution|null;
}

export interface EventListener {
  onTryingNewConfig(config: CameraConfigCandidate): void;
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
  config: CameraConfig|null = null;

  private readonly initialFacing: Facing|null;

  private readonly initialMode: Mode;

  private shouldSuspend = false;

  readonly capturePreferrer = new CaptureCandidatePreferrer();

  private readonly failedDevices = new Set<string>();

  private failedBySwPrivacySwitch = false;

  constructor(
      private readonly preview: Preview,
      private readonly modes: Modes,
      private readonly listener: EventListener,
      private readonly modeConstraints: ModeConstraints,
      facing: Facing|null,
  ) {
    this.initialMode = modeConstraints.mode;
    this.initialFacing = facing;
  }

  setShouldSuspend(value: boolean) {
    this.shouldSuspend = value;
  }

  getDeviceIdsSortedbyPreferredFacing(cameraInfo: CameraInfo): string[] {
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
    const facingPreference = util.getFacingPreference();
    if (this.initialFacing !== null) {
      facingPreference.unshift(this.initialFacing);
    }
    function preferFacingRank(deviceId: string) {
      if (facings === null) {
        return facingPreference.length;
      }
      const index = facingPreference.indexOf(facings[deviceId]);
      return index === -1 ? facingPreference.length : index;
    }

    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      return preferFacingRank(a) - preferFacingRank(b);
    });
    return sorted;
  }

  /**
   * Gets the video device ids sorted by preference.
   */
  private getDeviceIdCandidates(cameraInfo: CameraInfo): string[] {
    const deviceIds = this.getDeviceIdsSortedbyPreferredFacing(cameraInfo);
    // If there is no preferred device or the device is not in the list,
    // return devices sorted by preferred facing.
    if (this.config === null || !deviceIds.includes(this.config.deviceId)) {
      return deviceIds;
    }
    // Put the preferred device on the top of the list.
    function rotation(devices: string[], leftRotateNum: number): string[] {
      return devices.slice(leftRotateNum)
          .concat(devices.slice(0, leftRotateNum));
    }
    return rotation(deviceIds, deviceIds.indexOf(this.config.deviceId));
  }

  private async getModeCandidates(deviceId: string): Promise<Mode[]> {
    if (this.modeConstraints.kind === 'exact') {
      assert(await this.modes.isSupported(this.modeConstraints.mode, deviceId));
      return [this.modeConstraints.mode];
    }
    return this.modes.getModeCandidates(
        deviceId, this.config?.mode ?? this.initialMode);
  }

  private async *
      getConfigurationCandidates(cameraInfo: CameraInfo):
          AsyncIterable<ConfigureCandidate> {
    const deviceOperator = DeviceOperator.getInstance();
    const hasAudio = await this.isAudioInputAvailable();

    for (const deviceId of this.getDeviceIdCandidates(cameraInfo)) {
      for (const mode of await this.getModeCandidates(deviceId)) {
        let candidates: CaptureCandidate[];
        let photoResolutions;
        if (deviceOperator !== null) {
          assert(cameraInfo.camera3DevicesInfo !== null);
          candidates = this.capturePreferrer.getSortedCandidates(
              cameraInfo.camera3DevicesInfo, deviceId, mode, hasAudio);
          photoResolutions = await deviceOperator.getPhotoResolutions(deviceId);
        } else {
          candidates = [new FakeCameraCaptureCandidate(
              deviceId, mode === Mode.VIDEO, hasAudio)];
          photoResolutions = candidates.map((c) => c.resolution);
        }
        const maxResolution = photoResolutions.reduce(
            (maxR, r) =>
                r !== null && (maxR === null || r.area > maxR.area) ? r : maxR);
        for (const c of candidates) {
          const videoSnapshotResolution =
              expert.isEnabled(
                  expert.ExpertOption.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT) ?
              maxResolution :
              c.resolution;
          for (const constraints of c.getStreamConstraintsCandidates()) {
            yield {
              deviceId,
              mode,
              captureCandidate: c,
              constraints,
              videoSnapshotResolution,
            };
          }
        }
      }
    }
  }

  private async isAudioInputAvailable(): Promise<boolean> {
    const devices = await navigator.mediaDevices.enumerateDevices();
    return devices.some((device) => device.kind === 'audioinput');
  }

  /**
   * Checks if PTZ can be enabled.
   */
  private async checkEnablePtz(
      c: ConfigureCandidate, builtinPtzSupport: boolean): Promise<void> {
    const enablePtz = await (async () => {
      if (!this.preview.isSupportPtz()) {
        return false;
      }
      // In case of digital zoom PTZ or fake camera, PTZ is supported in all
      // capture and preview resolutions.
      if (!builtinPtzSupport) {
        return true;
      }
      const modeSupport = state.get(state.State.USE_FAKE_CAMERA) ||
          (c.captureCandidate.resolution !== null &&
           this.modes.isSupportPtz(
               c.mode,
               c.captureCandidate.resolution,
               this.preview.getResolution(),
               ));
      if (!modeSupport) {
        await this.preview.resetPtz();
        return false;
      }
      return true;
    })();
    state.set(state.State.ENABLE_PTZ, enablePtz);
  }

  async start(cameraInfo: CameraInfo): Promise<void> {
    await this.stopStreams();
    await this.startConfigure(cameraInfo);
  }

  /**
   * Clears the list of devices that previously failed to open and allows retry
   * to open devices even when the sw privacy switch is on.
   *
   */

  resetConfigurationFailure(): void {
    this.failedBySwPrivacySwitch = false;
    this.failedDevices.clear();
  }

  async startConfigure(cameraInfo: CameraInfo): Promise<void> {
    if (this.shouldSuspend) {
      throw new CameraSuspendError();
    }
    // CCA should attempt to open device at least once, even if the SW privacy
    // switch is on, to ensure the user receives a notification about the SW
    // privacy setting.
    if (this.failedBySwPrivacySwitch && util.isSWPrivacySwitchOn()) {
      // If a previous configuration failed due to the SW privacy switch being
      // on, and the switch is still on, skip this configuration attempt.
      throw new NoCameraError();
    }

    const deviceOperator = DeviceOperator.getInstance();
    state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);

    for await (const c of this.getConfigurationCandidates(cameraInfo)) {
      if (this.shouldSuspend) {
        throw new CameraSuspendError();
      }
      if (this.failedDevices.has(c.deviceId)) {
        // Check if the devices is released from other apps. If not,
        // we skip using it as a constraint to open a stream.
        const deviceOperator = DeviceOperator.getInstance();
        if (deviceOperator !== null) {
          const inUse = await deviceOperator.isDeviceInUse(c.deviceId);
          if (inUse) {
            continue;
          }
        }
      }
      let facing = c.deviceId !== null ?
          cameraInfo.getCamera3DeviceInfo(c.deviceId)?.facing ?? null :
          null;
      this.listener.onTryingNewConfig({
        deviceId: c.deviceId,
        facing,
        mode: c.mode,
        captureCandidate: c.captureCandidate,
      });
      this.modes.setCaptureParams(
          c.mode, c.constraints, c.captureCandidate.resolution,
          c.videoSnapshotResolution);
      try {
        if (deviceOperator !== null) {
          if (c.mode === Mode.PORTRAIT &&
              await deviceOperator.isDeviceInUse(c.deviceId)) {
            // TODO(b/326350233): Show a message to notify the user that the
            // device is in use.
            continue;
          }
        }
        await this.modes.prepareDevice();
        const factory = this.modes.getModeFactory(c.mode);
        await this.preview.open(c.constraints);
        // For non-ChromeOS VCD, the facing and device id can only be known
        // after preview is actually opened.
        facing = this.preview.getFacing();
        const deviceId = assertString(this.preview.getDeviceId());

        const builtinPtzSupport = cameraInfo.hasBuiltinPtzSupport(c.deviceId);
        await this.checkEnablePtz(c, builtinPtzSupport);
        factory.setPreviewVideo(this.preview.getVideo());
        factory.setFacing(facing);
        await this.modes.updateMode(factory);
        this.config = {
          deviceId,
          facing,
          mode: c.mode,
          captureCandidate: c.captureCandidate,
        };
        if (this.config.mode === Mode.VIDEO) {
          const fps = this.config.captureCandidate.getConstFps();
          state.set(state.State.FPS_30, fps === 30);
          state.set(state.State.FPS_60, fps === 60);
        }
        this.capturePreferrer.onUpdateConfig(this.config);
        await this.listener.onUpdateConfig(this.config);

        return;
      } catch (e) {
        await this.stopStreams();

        let errorToReport: Error;
        // Since OverconstrainedError is not an Error instance.
        if (e instanceof OverconstrainedError) {
          if (facing === Facing.EXTERNAL && e.constraint === 'deviceId') {
            // External camera configuration failed with OverconstrainedError
            // of deviceId means that the device is no longer available and is
            // likely caused by external camera disconnected. Ignore this
            // error.
            continue;
          }
          errorToReport =
              new Error(`${e.message} (constraint = ${e.constraint})`);
          errorToReport.name = 'OverconstrainedError';
        } else {
          assert(e instanceof Error);
          if (e.name === 'NotReadableError') {
            // TODO(b/187879603): Remove this hacked once we understand more
            // about such error.
            if (util.isSWPrivacySwitchOn()) {
              this.failedBySwPrivacySwitch = true;
              break;
            }
            let facing: Facing|null = null;
            let errorMessage: string = e.message;
            const deviceOperator = DeviceOperator.getInstance();
            if (deviceOperator !== null) {
              // We cannot get the camera facing from stream since it might
              // not be successfully opened. Therefore, we asked the camera
              // facing via Mojo API.
              facing = await deviceOperator.getCameraFacing(c.deviceId);
              // If 'NotReadableError' is thrown while the device is in use,
              // it means that the devices is used by Lacros.
              // In this case, we add it into `failedDevices` and skip using
              // it to open a stream until it is not in use.
              const inUse = await deviceOperator.isDeviceInUse(c.deviceId);
              if (inUse) {
                this.failedDevices.add(c.deviceId);
                errorMessage = 'Lacros is using the camera';
              }
            }
            errorToReport = new Error(`${errorMessage} (facing = ${facing})`);
            errorToReport.name = 'NotReadableError';
          } else {
            errorToReport = e;
          }
        }
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR, errorToReport);
      }
    }
    throw new NoCameraError();
  }

  /**
   * Stops extra stream and preview stream.
   */
  private async stopStreams() {
    await this.modes.clear();
    await this.preview.close();
  }
}

class Capturer {
  constructor(private readonly modes: Modes) {}

  async start(): Promise<[Promise<void>]> {
    assert(this.modes.current !== null);
    return this.modes.current.startCapture();
  }

  async stop() {
    assert(this.modes.current !== null);
    await this.modes.current.stopCapture();
  }

  takeVideoSnapshot() {
    if (this.modes.current instanceof Video) {
      this.modes.current.takeSnapshot();
    }
  }

  async toggleVideoRecordingPause(): Promise<void> {
    if (this.modes.current instanceof Video) {
      await this.modes.current.togglePaused();
    }
  }
}

enum OperationType {
  CAPTURE = 'capture',
  RECONFIGURE = 'reconfigure',
}

export class OperationScheduler {
  cameraInfo: CameraInfo|null = null;

  private pendingUpdateInfo: CameraInfo|null = null;

  private readonly firstInfoUpdate = new WaitableEvent();

  readonly reconfigurer: Reconfigurer;

  readonly capturer: Capturer;

  readonly modes = new Modes();

  private ongoingOperationType: OperationType|null = null;

  private pendingReconfigureWaiters: Array<CancelableEvent<void>> = [];

  private readonly togglePausedEventQueue = new AsyncJobQueue('drop');

  private readonly deviceMonitor = new DeviceMonitor((devices) => {
    const info = new CameraInfo(devices);
    if (this.ongoingOperationType !== null) {
      this.pendingUpdateInfo = info;
      return;
    }
    this.doUpdate(info);
  });

  constructor(
      private readonly listener: EventListener,
      preview: Preview,
      defaultFacing: Facing|null,
      modeConstraints: ModeConstraints,
  ) {
    this.reconfigurer = new Reconfigurer(
        preview,
        this.modes,
        listener,
        modeConstraints,
        defaultFacing,
    );
    this.capturer = new Capturer(this.modes);
  }

  async initialize(cameraViewUI: CameraViewUi): Promise<void> {
    this.modes.initialize(cameraViewUI);
    await this.deviceMonitor.deviceUpdate();
    await this.firstInfoUpdate.wait();
  }

  private doUpdate(cameraInfo: CameraInfo) {
    const isFirstUpdate = this.cameraInfo === null;
    this.cameraInfo = cameraInfo;
    if (cameraInfo.camera3DevicesInfo !== null) {
      this.reconfigurer.capturePreferrer.updateCapability(
          cameraInfo.camera3DevicesInfo);
    }
    this.listener.onUpdateCapability(cameraInfo);
    if (isFirstUpdate) {
      this.firstInfoUpdate.signal();
    }
  }

  async reconfigure(): Promise<void> {
    // If |startReconfigure| is invoked before the first update of camera info,
    // it will hit the assertion in |startReconfigure| and cause CCA hang.
    await this.firstInfoUpdate.wait();
    if (this.ongoingOperationType !== null) {
      const event = new CancelableEvent<void>();
      this.pendingReconfigureWaiters.push(event);
      await this.stopCapture();
      await event.wait();
    }
    await this.startReconfigure();
  }

  takeVideoSnapshot(): void {
    if (this.ongoingOperationType === OperationType.CAPTURE) {
      this.capturer.takeVideoSnapshot();
    }
  }

  toggleVideoRecordingPause(): void {
    this.togglePausedEventQueue.push(async () => {
      if (this.ongoingOperationType !== OperationType.CAPTURE) {
        return;
      }
      try {
        await this.capturer.toggleVideoRecordingPause();
      } catch (e) {
        error.reportError(ErrorType.RESUME_PAUSE_FAILURE, ErrorLevel.ERROR, e);
      }
    });
  }

  private clearPendingReconfigureWaiters() {
    for (const waiter of this.pendingReconfigureWaiters) {
      waiter.signal();
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
      const promise = this.startReconfigure();
      for (const waiter of this.pendingReconfigureWaiters) {
        waiter.signalAs(promise);
      }
      this.pendingReconfigureWaiters = [];
    }
  }

  async startCapture(): Promise<[Promise<void>]|null> {
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

  async stopCapture(): Promise<void> {
    if (this.ongoingOperationType !== OperationType.CAPTURE) {
      return;
    }
    await this.togglePausedEventQueue.flush();
    await this.capturer.stop();
  }

  private async startReconfigure(): Promise<void> {
    assert(this.ongoingOperationType === null);
    this.ongoingOperationType = OperationType.RECONFIGURE;

    const cameraInfo = assertInstanceof(this.cameraInfo, CameraInfo);
    const startPromise = this.reconfigurer.start(cameraInfo);
    // This is for processing after the current reconfigure is done.
    void (async () => {
      try {
        await startPromise;
      } catch (e) {
        this.clearPendingReconfigureWaiters();
      } finally {
        this.finishOperation();
      }
    })();
    // Only returns the "start" part, so the returned promise is resolved
    // before all the waiters are resolved to keep the order correct.
    await startPromise;
  }
}
