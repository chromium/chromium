// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertExists,
} from '../assert.js';
import * as expert from '../expert.js';
import {Point} from '../geometry.js';
import * as metrics from '../metrics.js';
import * as loadTimeData from '../models/load_time_data.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {LidState, ScreenState} from '../mojo/type.js';
import * as nav from '../nav.js';
import {PerfLogger} from '../perf.js';
import * as state from '../state.js';
import {
  AspectRatioSet,
  CameraSuspendError,
  Facing,
  Mode,
  PerfEvent,
  PhotoResolutionLevel,
  PreviewVideo,
  Resolution,
  VideoResolutionLevel,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {WarningType} from '../views/warning.js';
import {WaitableEvent} from '../waitable_event.js';
import {windowController} from '../window_controller.js';

import {EventListener, OperationScheduler} from './camera_operation.js';
import {VideoCaptureCandidate} from './capture_candidate.js';
import {Preview} from './preview.js';
import {PtzController} from './ptz_controller.js';
import {
  CameraConfig,
  CameraInfo,
  CameraUi,
  CameraViewUi,
  ModeConstraints,
  PhotoAspectRatioOptionListener,
  PhotoResolutionOptionListener,
  VideoResolutionOptionListener,
} from './type.js';

class ResumeStateWatchdog {
  // This is definitely assigned in this.start() in the first statement of the
  // while loop.
  private trialDone!: WaitableEvent<boolean>;

  private succeed = false;

  constructor(private readonly doReconfigure: () => Promise<boolean>) {
    // This is for watchdog running in the background.
    // TODO(pihsun): Move this out of constructor.
    void this.start();
  }

  private async start() {
    while (!this.succeed) {
      this.trialDone = new WaitableEvent<boolean>();
      await util.sleep(100);
      this.succeed = await this.doReconfigure();
      this.trialDone.signal(this.succeed);
    }
  }

  /**
   * Waits for the next unfinished reconfigure result.
   *
   * @return The reconfigure is succeed or failed.
   */
  async waitNextReconfigure(): Promise<boolean> {
    return this.trialDone.wait();
  }
}

/**
 * Manages usages of all camera operations.
 * TODO(b/209726472): Move more camera logic in camera view to here.
 */
export class CameraManager implements EventListener {
  private hasExternalScreen = false;

  private screenOffAuto = false;

  private cameraAvailable = false;

  /**
   * Whether the device is in locked state.
   */
  private locked = false;

  private suspendRequested = false;

  private readonly scheduler: OperationScheduler;

  private watchdog: ResumeStateWatchdog|null = null;

  private readonly cameraUis: CameraUi[] = [];

  private readonly preview: Preview;

  constructor(
      defaultFacing: Facing|null,
      modeConstraints: ModeConstraints,
  ) {
    this.preview = new Preview(async () => {
      await this.reconfigure();
    }, () => this.useSquareResolution());

    this.scheduler = new OperationScheduler(
        this,
        this.preview,
        defaultFacing,
        modeConstraints,
    );

    document.addEventListener('visibilitychange', async () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (!recording) {
        await this.maybeSuspendResumeCamera();
      }
    });

    expert.addObserver(
        expert.ExpertOption.SHOW_ALL_RESOLUTIONS,
        async () => {
          // Rebuilds the options to adapt to the new state. Then, reconfigure
          // the stream so that it will apply the new resolution order. At last,
          // update the checked status of the resolution options.
          this.scheduler.reconfigurer.capturePreferrer.buildOptions();
          await this.tryReconfigure(() => {/* Do nothing */});
        },
    );
  }

  getCameraInfo(): CameraInfo {
    return assertExists(this.scheduler.cameraInfo);
  }

  getDeviceId(): string|null {
    return this.scheduler.reconfigurer.config?.deviceId ?? null;
  }

  getPreviewVideo(): PreviewVideo {
    return this.preview.getVideo();
  }

  getAudioTrack(): MediaStreamTrack|null {
    return this.getPreviewVideo().getStream().getAudioTracks()[0] ?? null;
  }

  /**
   * USB camera vid:pid identifier of the opened stream.
   *
   * @return Identifier formatted as "vid:pid" or null for non-USB camera.
   */
  getVidPid(): string|null {
    return this.preview.getVidPid();
  }

  getPreviewResolution(): Resolution {
    const {video} = this.getPreviewVideo();
    const {videoWidth, videoHeight} = video;
    if (this.useSquareResolution()) {
      const size = Math.min(videoWidth, videoHeight);
      return new Resolution(size, size);
    }
    return new Resolution(videoWidth, videoHeight);
  }

  getCaptureResolution(): Resolution|null {
    assert(this.scheduler.reconfigurer.config !== null);
    return this.scheduler.reconfigurer.config.captureCandidate.resolution;
  }

  getConstFps(): number|null {
    assert(this.scheduler.reconfigurer.config !== null);
    const c = this.scheduler.reconfigurer.config.captureCandidate;
    if (!(c instanceof VideoCaptureCandidate)) {
      return null;
    }
    return c.constFps;
  }

  async getSupportedModes(deviceId: string|null): Promise<Mode[]> {
    const modes: Mode[] = [];
    for (const mode of Object.values(Mode)) {
      if (await this.scheduler.modes.isSupported(mode, deviceId)) {
        modes.push(mode);
      }
    }
    return modes;
  }

  async onUpdateConfig(config: CameraConfig): Promise<void> {
    for (const ui of this.cameraUis) {
      await ui.onUpdateConfig?.(config);
    }
  }

  onTryingNewConfig(config: CameraConfig): void {
    for (const ui of this.cameraUis) {
      ui.onTryingNewConfig?.(config);
    }
  }

  onUpdateCapability(cameraInfo: CameraInfo): void {
    for (const ui of this.cameraUis) {
      ui.onUpdateCapability?.(cameraInfo);
    }
  }

  registerCameraUi(ui: CameraUi): void {
    this.cameraUis.push(ui);
  }

  /**
   * @return Whether window is put to background in tablet mode.
   */
  private isTabletBackground(): boolean {
    return state.get(state.State.TABLET) &&
        document.visibilityState === 'hidden';
  }

  /**
   * @return If the App window is invisible to user with respect to screen off
   *     state.
   */
  private get screenOff(): boolean {
    return this.screenOffAuto && !this.hasExternalScreen;
  }

  async initialize(cameraViewUI: CameraViewUi): Promise<void> {
    const helper = ChromeHelper.getInstance();

    function setTablet(isTablet: boolean) {
      state.set(state.State.TABLET, isTablet);
    }
    const isTablet = await helper.initTabletModeMonitor(setTablet);
    setTablet(isTablet);

    function setLidClosed(lidState: LidState) {
      state.set(state.State.LID_CLOSED, lidState === LidState.kClosed);
    }
    const lidState = await helper.initLidStateMonitor(setLidClosed);
    setLidClosed(lidState);

    function setSwPirvacySwitchOn(isSWPrivacySwitchOn: boolean) {
      state.set(state.State.SW_PRIVACY_SWITCH_ON, isSWPrivacySwitchOn);
    }
    const isSwPrivacySwitchOn =
        await helper.initSwPrivacySwitchMonitor(setSwPirvacySwitchOn);
    setSwPirvacySwitchOn(isSwPrivacySwitchOn);

    const handleScreenLockedChange = async (isScreenLocked: boolean) => {
      this.locked = isScreenLocked;
      await this.maybeSuspendResumeCamera();
    };

    this.locked =
        await helper.initScreenLockedMonitor(handleScreenLockedChange);

    const handleScreenStateChange = async () => {
      await this.maybeSuspendResumeCamera();
    };

    const updateScreenOffAuto = async (screenState: ScreenState) => {
      const isOffAuto = screenState === ScreenState.kOffAuto;
      if (this.screenOffAuto !== isOffAuto) {
        this.screenOffAuto = isOffAuto;
        await handleScreenStateChange();
      }
    };
    const screenState =
        await helper.initScreenStateMonitor(updateScreenOffAuto);

    const updateExternalScreen = async (hasExternalScreen: boolean) => {
      if (this.hasExternalScreen !== hasExternalScreen) {
        this.hasExternalScreen = hasExternalScreen;
        await handleScreenStateChange();
      }
    };
    const hasExternalScreen =
        await helper.initExternalScreenMonitor(updateExternalScreen);

    this.screenOffAuto = screenState === ScreenState.kOffAuto;
    this.hasExternalScreen = hasExternalScreen;

    await this.scheduler.initialize(cameraViewUI);
  }

  requestSuspend(): Promise<void> {
    this.suspendRequested = true;
    return this.maybeSuspendResumeCamera();
  }

  requestResume(): Promise<void> {
    this.suspendRequested = false;
    return this.maybeSuspendResumeCamera();
  }

  // Checks the state of CCA and suspends or resumes the camera accordingly.
  async maybeSuspendResumeCamera(): Promise<void> {
    const shouldSuspend = this.shouldSuspend();
    if (state.get(state.State.SUSPEND) === shouldSuspend) {
      return;
    }
    await this.reconfigure();
  }

  /**
   * Switches to the next available camera device.
   */
  switchCamera(): Promise<void>|null {
    const perfLogger = PerfLogger.getInstance();
    const promise = this.tryReconfigure(() => {
      perfLogger.start(PerfEvent.CAMERA_SWITCHING);
      const deviceIds =
          this.scheduler.reconfigurer.getDeviceIdsSortedbyPreferredFacing(
              this.getCameraInfo());
      if (deviceIds.length === 0) {
        return;
      }
      let index =
          deviceIds.findIndex((deviceId) => deviceId === this.getDeviceId());
      // findIndex() may return -1, which means the device is not in the list.
      // In this case, we will try to switch to the preferred facing device.
      index = (index + 1) % deviceIds.length;
      assertExists(this.scheduler.reconfigurer.config).deviceId =
          deviceIds[index];
    });
    if (promise === null) {
      return null;
    }
    return promise.then((succeed) => {
      perfLogger.stop(PerfEvent.CAMERA_SWITCHING, {hasError: !succeed});
      metrics.sendOpenCameraEvent(this.getVidPid());
    });
  }

  switchMode(mode: Mode): Promise<boolean>|null {
    return this.tryReconfigure(() => {
      assert(this.scheduler.reconfigurer.config !== null);
      this.scheduler.reconfigurer.config.mode = mode;
    });
  }

  private async setCapturePref(deviceId: string, setPref: () => void):
      Promise<boolean> {
    if (!this.cameraAvailable) {
      return false;
    }
    if (deviceId !== this.getDeviceId()) {
      // Changing the configure of the camera not currently opened, thus no
      // reconfiguration are required.
      setPref();
      return true;
    }
    return this.tryReconfigure(setPref) ?? false;
  }

  addPhotoResolutionOptionListener(listener: PhotoResolutionOptionListener):
      void {
    this.scheduler.reconfigurer.capturePreferrer
        .addPhotoResolutionOptionListener(listener);
  }

  addPhotoAspectRatioOptionListener(listener: PhotoAspectRatioOptionListener):
      void {
    this.scheduler.reconfigurer.capturePreferrer
        .addPhotoAspectRatioOptionListener(listener);
  }

  addVideoResolutionOptionListener(listener: VideoResolutionOptionListener):
      void {
    this.scheduler.reconfigurer.capturePreferrer
        .addVideoResolutionOptionListener(listener);
  }

  async setPrefPhotoResolutionLevel(
      deviceId: string, level: PhotoResolutionLevel): Promise<void> {
    await this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefPhotoResolutionLevel(
          deviceId, level);
    });
  }

  async setPrefPhotoAspectRatioSet(
      deviceId: string, aspectRatioSet: AspectRatioSet): Promise<void> {
    await this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefPhotoAspectRatioSet(
          deviceId, aspectRatioSet);
    });
  }

  async setPrefVideoResolutionLevel(
      deviceId: string, level: VideoResolutionLevel): Promise<void> {
    await this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefVideoResolutionLevel(
          deviceId, level);
    });
  }

  /**
   * Used when showing all resolutions.
   */
  async setPrefPhotoResolution(deviceId: string, resolution: Resolution):
      Promise<void> {
    await this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefPhotoResolution(
          deviceId, resolution);
    });
  }

  /**
   * Used when showing all resolutions.
   */
  async setPrefVideoResolution(deviceId: string, resolution: Resolution):
      Promise<void> {
    await this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefVideoResolution(
          deviceId, resolution);
    });
  }

  /**
   * Sets fps of constant video recording on currently opened camera and
   * resolution.
   */
  setPrefVideoConstFps(
      deviceId: string, level: VideoResolutionLevel, fps: number,
      shouldReconfigure: boolean): Promise<boolean>|null {
    // We only need to reconfigure the stream if the FPS preference has been
    // changed for selected resolution level.
    if (shouldReconfigure) {
      return this.setCapturePref(deviceId, () => {
        this.scheduler.reconfigurer.capturePreferrer.setPrefVideoConstFps(
            deviceId, level, fps, shouldReconfigure);
      });
    } else {
      this.scheduler.reconfigurer.capturePreferrer.setPrefVideoConstFps(
          deviceId, level, fps, shouldReconfigure);
      return null;
    }
  }

  getPhotoResolutionLevel(resolution: Resolution): PhotoResolutionLevel {
    return this.scheduler.reconfigurer.capturePreferrer.getPhotoResolutionLevel(
        resolution);
  }

  getVideoResolutionLevel(resolution: Resolution): VideoResolutionLevel {
    return this.scheduler.reconfigurer.capturePreferrer.getVideoResolutionLevel(
        resolution);
  }

  getAspectRatioSet(resolution: Resolution): AspectRatioSet {
    if (this.useSquareResolution()) {
      return AspectRatioSet.RATIO_SQUARE;
    }
    return util.toAspectRatioSet(resolution);
  }

  getZoomRatio(): number {
    return this.preview.getZoomRatio();
  }

  /**
   * Applies point of interest to the stream.
   *
   * @param point The point in normalize coordidate system, which means both
   *     |x| and |y| are in range [0, 1).
   */
  setPointOfInterest(point: Point): Promise<void> {
    return this.preview.setPointOfInterest(point);
  }

  getPtzController(): PtzController {
    return this.preview.getPtzController();
  }

  resetPtz(): Promise<void> {
    return this.preview.resetPtz();
  }

  /**
   * Whether the photo taking should be done by using preview frame as photo.
   * This is the workaround for b/184089334 to avoid mismatch between preview
   * and photo results in some PTZ cameras.
   */
  shouldUsePreviewAsPhoto(): boolean {
    const deviceId = this.getDeviceId();
    if (deviceId === null) {
      return false;
    }
    return state.get(state.State.ENABLE_PTZ) &&
        this.getCameraInfo().hasBuiltinPtzSupport(deviceId);
  }

  /**
   * Whether app window is suspended.
   */
  private shouldSuspend(): boolean {
    return this.locked || windowController.isMinimized() ||
        this.suspendRequested || this.screenOff || this.isTabletBackground();
  }

  async startCapture(): Promise<[Promise<void>]> {
    this.setCameraAvailable(false);
    try {
      const captureDone = await this.scheduler.startCapture();
      return assertExists(captureDone);
    } finally {
      this.setCameraAvailable(true);
    }
  }

  async stopCapture(): Promise<void> {
    await this.scheduler.stopCapture();
  }

  takeVideoSnapshot(): void {
    this.scheduler.takeVideoSnapshot();
  }

  toggleVideoRecordingPause(): void {
    this.scheduler.toggleVideoRecordingPause();
  }

  useSquareResolution(): boolean {
    if (!(state.get(Mode.PHOTO) || state.get(Mode.PORTRAIT))) {
      return false;
    }
    const deviceId = this.getDeviceId();
    if (deviceId === null) {
      return false;
    }
    return this.scheduler.reconfigurer.capturePreferrer.preferSquarePhoto(
        deviceId);
  }

  private setCameraAvailable(available: boolean): void {
    if (available === this.cameraAvailable) {
      return;
    }
    this.cameraAvailable = available;
    for (const ui of this.cameraUis) {
      if (this.cameraAvailable) {
        ui.onCameraAvailable?.();
      } else {
        ui.onCameraUnavailable?.();
      }
    }
  }

  private tryReconfigure(setNewConfig: () => void): Promise<boolean>|null {
    if (!this.cameraAvailable) {
      return null;
    }
    setNewConfig();
    return this.reconfigure();
  }

  async reconfigure(): Promise<boolean> {
    // TODO(pihsun): This (and tryReconfigure) is being called by many sync
    // callback. Revisit this to push reconfigure jobs on an AsyncJobQueue and
    // returns result in a AsyncJob instead of directly returning a Promise?
    if (this.watchdog !== null) {
      if (!await this.watchdog.waitNextReconfigure()) {
        return false;
      }
      // The watchdog.waitNextReconfigure() only return the most recent
      // reconfigure result which may not reflect the setting before calling it.
      // Thus still fallthrough here to start another reconfigure.
    }
    this.scheduler.reconfigurer.resetConfigurationFailure();
    return this.doReconfigure();
  }

  private async doReconfigure(): Promise<boolean> {
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.setCameraAvailable(false);
    const shouldSuspend = this.shouldSuspend();
    this.scheduler.reconfigurer.setShouldSuspend(shouldSuspend);
    state.set(state.State.SUSPEND, shouldSuspend);
    const perfLogger = PerfLogger.getInstance();
    if (loadTimeData.isCCADisallowed()) {
      nav.open(ViewName.WARNING, WarningType.DISABLED_CAMERA);
      perfLogger.interrupt();
      return false;
    }
    try {
      await this.scheduler.reconfigure();
    } catch (e) {
      if (e instanceof CameraSuspendError) {
        // Bypass this error as it's intended.
      } else {
        // Keep trying reconfiguring until there's an available camera.
        if (this.watchdog === null) {
          // TODO(b/209726472): Move nav out of this module.
          nav.open(ViewName.WARNING, WarningType.NO_CAMERA);
          this.watchdog = new ResumeStateWatchdog(() => this.doReconfigure());
        }
      }
      perfLogger.interrupt();
      return false;
    }

    // TODO(b/209726472): Move nav out of this module.
    nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
    this.watchdog = null;
    state.set(state.State.CAMERA_CONFIGURING, false);
    this.setCameraAvailable(true);
    return true;
  }
}
