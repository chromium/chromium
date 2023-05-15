// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../animation.js';
import {
  assert,
  assertInstanceof,
  assertNotReached,
} from '../assert.js';
import {
  CameraConfig,
  CameraManager,
  CameraViewUI,
  getDefaultScanCorners,
  GifResult,
  PhotoResult,
  setAvc1Parameters,
  VideoResult,
} from '../device/index.js';
import {TimeLapseResult} from '../device/mode/video';
import * as dom from '../dom.js';
import * as error from '../error.js';
import * as expert from '../expert.js';
import {I18nString} from '../i18n_string.js';
import * as metrics from '../metrics.js';
import {Filenamer} from '../models/file_namer.js';
import {getI18nMessage} from '../models/load_time_data.js';
import {ResultSaver} from '../models/result_saver.js';
import {VideoSaver} from '../models/video_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {ToteMetricFormat} from '../mojo/type.js';
import * as nav from '../nav.js';
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import {speak} from '../spoken_msg.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  CameraSuspendError,
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  LowStorageDialogType,
  LowStorageError,
  MimeType,
  Mode,
  PerfEvent,
  PortraitModeProcessError,
  Resolution,
  Rotation,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {Layout} from './camera/layout.js';
import {Options} from './camera/options.js';
import {ScanOptions} from './camera/scan_options.js';
import * as timertick from './camera/timertick.js';
import {VideoEncoderOptions} from './camera/video_encoder_options.js';
import {Dialog} from './dialog.js';
import {DocumentReview} from './document_review.js';
import {OptionPanel} from './option_panel.js';
import {PTZPanel} from './ptz_panel.js';
import * as review from './review.js';
import {PrimarySettings} from './settings/primary.js';
import {View} from './view.js';
import {WarningType} from './warning.js';

/**
 * Camera-view controller.
 */
export class Camera extends View implements CameraViewUI {
  private readonly documentReview: DocumentReview;

  private currentLowStorageType: LowStorageDialogType|null = null;

  private readonly lowStorageDialogView: Dialog;

  private readonly subViews: View[];

  /**
   * Layout handler for the camera view.
   */
  private readonly layoutHandler: Layout;

  private readonly scanOptions: ScanOptions;

  private readonly videoEncoderOptions =
      new VideoEncoderOptions((parameters) => setAvc1Parameters(parameters));

  /**
   * Clock-wise rotation that needs to be applied to the recorded video in
   * order for the video to be replayed in upright orientation.
   */
  private outputVideoRotation = 0;

  /**
   * Device id of video device of active preview stream. Sets to null when
   * preview become inactive.
   */
  private activeDeviceId: string|null = null;

  protected readonly review = new review.Review();

  protected facing: Facing|null = null;

  protected shutterType = metrics.ShutterType.UNKNOWN;

  /**
   * Event for tracking camera availability state.
   */
  private cameraReady = new WaitableEvent();

  /**
   * Promise for the current take of photo or recording.
   */
  private take: Promise<void>|null = null;

  private readonly modesGroup = dom.get('#modes-group', HTMLElement);

  constructor(
      protected readonly resultSaver: ResultSaver,
      protected readonly cameraManager: CameraManager,
      readonly perfLogger: PerfLogger,
  ) {
    super(ViewName.CAMERA);
    this.documentReview = new DocumentReview(resultSaver);
    this.lowStorageDialogView = new Dialog(ViewName.LOW_STORAGE_DIALOG, {
      onNegativeButtonClicked: () => this.openStorageManagement(),
    });
    this.subViews = [
      new PrimarySettings(this.cameraManager),
      new OptionPanel(),
      new PTZPanel(),
      this.review,
      this.documentReview,
      this.lowStorageDialogView,
      new View(ViewName.FLASH),
    ];

    this.layoutHandler = new Layout(this.cameraManager);

    this.scanOptions = new ScanOptions(this.cameraManager);

    // Options for the camera.
    // Put it here for it controls the UI visually under camera view but it
    // currently won't interact with the view. To prevent typescript checker
    // complainting about the unused reference, it's left here without any
    // reference point to it.
    new Options(this.cameraManager);

    /**
     * Gets type of ways to trigger shutter from click event.
     */
    function getShutterType(e: MouseEvent) {
      if (e.clientX === 0 && e.clientY === 0) {
        return metrics.ShutterType.KEYBOARD;
      }
      return (e.sourceCapabilities?.firesTouchEvents ?? false) ?
          metrics.ShutterType.TOUCH :
          metrics.ShutterType.MOUSE;
    }
    const photoShutter = dom.get('#start-takephoto', HTMLButtonElement);
    photoShutter.addEventListener('click', (e) => {
      this.beginTake(getShutterType(e));
    });
    function checkPhotoShutter() {
      const disabled = state.get(state.State.CAMERA_CONFIGURING) ||
          state.get(state.State.TAKING);
      photoShutter.disabled = disabled;
    }
    state.addObserver(state.State.CAMERA_CONFIGURING, checkPhotoShutter);
    state.addObserver(state.State.TAKING, checkPhotoShutter);

    dom.get('#stop-takephoto', HTMLButtonElement)
        .addEventListener('click', () => this.endTake());

    const videoShutter = dom.get('#recordvideo', HTMLButtonElement);
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake(getShutterType(e));
      } else {
        this.endTake();
      }
    });
    function checkVideoShutter() {
      const disabled = state.get(state.State.CAMERA_CONFIGURING) &&
          !state.get(state.State.TAKING);
      videoShutter.disabled = disabled;
    }
    state.addObserver(state.State.CAMERA_CONFIGURING, checkVideoShutter);
    state.addObserver(state.State.TAKING, checkVideoShutter);

    const videoSnapshotButton = dom.get('#video-snapshot', HTMLButtonElement);
    videoSnapshotButton.addEventListener('click', () => {
      this.cameraManager.takeVideoSnapshot();
    });
    function checkVideoSnapshotButton() {
      const disabled = state.get(state.State.SNAPSHOTTING);
      videoSnapshotButton.disabled = disabled;
    }
    state.addObserver(state.State.SNAPSHOTTING, checkVideoSnapshotButton);

    const pauseShutter = dom.get('#pause-recordvideo', HTMLButtonElement);
    pauseShutter.addEventListener('click', () => {
      this.cameraManager.toggleVideoRecordingPause();
    });

    // TODO(shik): Tune the timing for playing video shutter button animation.
    // Currently the |TAKING| state is ended when the file is saved.
    util.bindElementAriaLabelWithState({
      element: videoShutter,
      state: state.State.TAKING,
      onLabel: I18nString.RECORD_VIDEO_STOP_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_START_BUTTON,
    });
    util.bindElementAriaLabelWithState({
      element: pauseShutter,
      state: state.State.RECORDING_PAUSED,
      onLabel: I18nString.RECORD_VIDEO_RESUME_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_PAUSE_BUTTON,
    });

    this.cameraManager.registerCameraUI({
      onTryingNewConfig: (config: CameraConfig) => {
        this.updateModeUI(config.mode);
        this.updateShutterLabel(config.mode);
      },
      onUpdateConfig: async (config: CameraConfig) => {
        nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
        this.facing = config.facing;
        this.updateActiveCamera(config.deviceId);

        // Update current mode.
        const supportedModes =
            await this.cameraManager.getSupportedModes(config.deviceId);
        const items = dom.getAll('div.mode-item', HTMLDivElement);
        let first: HTMLElement|null = null;
        let last: HTMLElement|null = null;
        for (const el of items) {
          const radio = dom.getFrom(el, 'input[type=radio]', HTMLInputElement);
          const supported = supportedModes.includes(
              util.assertEnumVariant(Mode, radio.dataset['mode']));
          el.classList.toggle('hide', !supported);
          if (supported) {
            if (first === null) {
              first = el;
            }
            last = el;
          }
        }
        for (const el of items) {
          el.classList.toggle('first', el === first);
          el.classList.toggle('last', el === last);
        }
      },
      onCameraUnavailable: () => {
        this.cameraReady = new WaitableEvent();
      },
      onCameraAvailable: () => {
        this.cameraReady.signal();
      },
    });

    const checkModesGroupDisabled = () => {
      const disabled =
          !state.get(state.State.STREAMING) || state.get(state.State.TAKING);
      const modes =
          dom.getAllFrom(this.modesGroup, '.mode-item>input', HTMLInputElement);
      for (const mode of modes) {
        // Use data-disabled here because:
        // 1. `mode.disabled = true` loses focus on the element.
        // 2. `mode.setAttribute('aria-disabled', 'true')` makes ChromeVox
        //    always announce the element is disabled.
        mode.dataset['disabled'] = String(disabled);
      }
    };
    state.addObserver(state.State.STREAMING, checkModesGroupDisabled);
    state.addObserver(state.State.TAKING, checkModesGroupDisabled);
    checkModesGroupDisabled();

    for (const el of dom.getAll('.mode-item>input', HTMLInputElement)) {
      el.addEventListener('click', (event) => {
        if (!this.cameraReady.isSignaled() ||
            el.dataset['disabled'] === 'true') {
          event.preventDefault();
        }
      });
      el.addEventListener('change', async () => {
        if (el.checked) {
          const mode = util.assertEnumVariant(Mode, el.dataset['mode']);
          this.updateModeUI(mode);
          this.updateShutterLabel(mode);
          state.set(PerfEvent.MODE_SWITCHING, true);
          const isSuccess = await this.cameraManager.switchMode(mode) ?? false;
          state.set(PerfEvent.MODE_SWITCHING, false, {hasError: !isSuccess});
        }
      });
    }
    dom.get('#back-to-review-document', HTMLButtonElement)
        .addEventListener(
            'click',
            () => {
              this.reviewDocument();
            },
        );
  }

  /**
   * Initializes camera view.
   */
  async initialize(): Promise<void> {
    expert.addObserver(
        expert.ExpertOption.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT,
        () => this.cameraManager.reconfigure());
    expert.addObserver(
        expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING,
        () => this.cameraManager.reconfigure());
    expert.addObserver(
        expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING_CHROME,
        () => this.cameraManager.reconfigure());
    expert.addObserver(
        expert.ExpertOption.ENABLE_PTZ_FOR_BUILTIN,
        () => this.cameraManager.reconfigure());

    this.initVideoEncoderOptions();
    this.initScanMode();
  }

  /**
   * Gets current facing after |initialize()|.
   */
  protected getFacing(): Facing {
    return util.assertEnumVariant(Facing, this.facing);
  }

  private updateModeUI(mode: Mode) {
    for (const m of Object.values(Mode)) {
      state.set(m, m === mode);
    }
    const element =
        dom.get(`.mode-item>input[data-mode=${mode}]`, HTMLInputElement);
    element.checked = true;
    const wrapper = assertInstanceof(element.parentElement, HTMLDivElement);
    const scrollLeft = wrapper.offsetLeft -
        (this.modesGroup.offsetWidth - wrapper.offsetWidth) / 2;
    this.modesGroup.scrollTo({
      left: scrollLeft,
      top: 0,
      behavior: 'smooth',
    });
  }

  private updateShutterLabel(mode: Mode) {
    const element = dom.get('#start-takephoto', HTMLButtonElement);
    const label =
        mode === 'scan' ? I18nString.SCAN_BUTTON : I18nString.TAKE_PHOTO_BUTTON;
    element.setAttribute('i18n-label', label);
    element.setAttribute('aria-label', getI18nMessage(label));
  }

  private initVideoEncoderOptions() {
    const options = this.videoEncoderOptions;
    this.cameraManager.registerCameraUI({
      onUpdateConfig: () => {
        if (state.get(Mode.VIDEO)) {
          const {width, height, frameRate} =
              this.cameraManager.getPreviewVideo().getVideoSettings();
          assert(width !== undefined);
          assert(height !== undefined);
          assert(frameRate !== undefined);
          options.updateValues(new Resolution(width, height), frameRate);
        }
      },
    });
    options.initialize();
  }

  private async initScanMode() {
    const isLoaded = await this.scanOptions.checkDocumentModeReadiness();
    if (!isLoaded) {
      return;
    }
    // When entering document mode, refocus to shutter button for letting user
    // to take document photo with space key as shortcut. See b/196907822.
    const checkRefocus = () => {
      if (!state.get(state.State.CAMERA_CONFIGURING) && state.get(Mode.SCAN) &&
          this.scanOptions.isDocumentModeEnabled()) {
        this.focusShutterButton();
      }
    };
    state.addObserver(state.State.CAMERA_CONFIGURING, checkRefocus);
    this.scanOptions.addOnChangeListener(() => checkRefocus());
  }

  override getSubViews(): View[] {
    return this.subViews;
  }

  private focusShutterButton(): void {
    if (!nav.isTopMostView(this.name)) {
      return;
    }
    // Avoid focusing invisible shutters.
    for (const btn of dom.getAll('button.shutter', HTMLButtonElement)) {
      if (btn.offsetParent !== null) {
        btn.focus();
      }
    }
  }

  private async defaultFocus(): Promise<void> {
    await this.cameraReady.wait();

    // Check the view is still on the top after await.
    if (!nav.isTopMostView(ViewName.CAMERA)) {
      return;
    }

    this.focusShutterButton();
  }

  override onShownAsTop(): void {
    this.defaultFocus();
  }

  override onUncoveredAsTop(viewName: ViewName): void {
    if ([ViewName.SETTINGS, ViewName.OPTION_PANEL].includes(viewName)) {
      // Don't refocus on shutter button when coming back from setting menu.
      super.onUncoveredAsTop(viewName);
    } else {
      this.setFocusable();
      this.defaultFocus();
    }
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   *
   * @param shutterType The shutter is triggered by which shutter type.
   * @return Promise resolved when take action completes. Returns null if CCA
   *     can't start take action.
   */
  beginTake(shutterType: metrics.ShutterType): Promise<void>|null {
    if (state.get(state.State.CAMERA_CONFIGURING) ||
        state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.shutterType = shutterType;
    // Refocus the visible shutter button for ChromeVox.
    this.focusShutterButton();
    this.take = (async () => {
      let hasError = false;
      try {
        // Record and keep the rotation only at the instance the user starts the
        // capture. Users may change the device orientation while taking video.
        const cameraFrameRotation = await (async () => {
          const deviceOperator = DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return 0;
          }
          assert(this.activeDeviceId !== null);
          return deviceOperator.getCameraFrameRotation(this.activeDeviceId);
        })();
        // Translate the camera frame rotation back to the UI rotation, which is
        // what we need to rotate the captured video with.
        this.outputVideoRotation = (360 - cameraFrameRotation) % 360;
        await timertick.start();
        const [captureDone] = await this.cameraManager.startCapture();
        await captureDone;
      } catch (e) {
        if (e instanceof LowStorageError) {
          this.showLowStorageDialog(LowStorageDialogType.CANNOT_START);
          // Don't send capture error.
          return;
        }
        hasError = true;
        if (e instanceof CanceledError || e instanceof CameraSuspendError) {
          return;
        }
        error.reportError(
            ErrorType.START_CAPTURE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
      } finally {
        this.take = null;
        state.set(state.State.TAKING, false, {
          hasError,
          facing: this.getFacing(),
        });
        // Refocus the visible shutter button for ChromeVox.
        this.focusShutterButton();
      }
    })();
    return this.take;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any).
   *
   * @return Promise for the operation.
   */
  private async endTake(): Promise<void> {
    timertick.cancel();
    this.cameraManager.stopCapture();
    await this.take;
  }

  private async checkPhotoResult<T>(pendingPhotoResult: Promise<T>):
      Promise<T> {
    try {
      return await pendingPhotoResult;
    } catch (e) {
      this.onPhotoError();
      throw e;
    }
  }

  async handleVideoSnapshot({resolution, blob, timestamp, metadata}:
                                PhotoResult): Promise<void> {
    metrics.sendCaptureEvent({
      facing: this.getFacing(),
      resolution,
      shutterType: this.shutterType,
      isVideoSnapshot: true,
      resolutionLevel: this.cameraManager.getVideoResolutionLevel(resolution),
      aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
    });
    try {
      const name = (new Filenamer(timestamp)).newImageName();
      await this.resultSaver.savePhoto(
          blob, ToteMetricFormat.PHOTO, name, metadata);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  async onPhotoError(): Promise<void> {
    toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
  }

  async onNoPortrait(): Promise<void> {
    toast.show(I18nString.ERROR_MSG_TAKE_PORTRAIT_BOKEH_PHOTO_FAILED);
  }

  async cropIfUsingSquareResolution(result: Promise<PhotoResult>):
      Promise<PhotoResult> {
    if (!this.cameraManager.useSquareResolution()) {
      return result;
    }
    const photoResult = await result;
    const croppedBlob = await util.cropSquare(photoResult.blob);
    return {
      ...photoResult,
      blob: croppedBlob,
    };
  }

  async onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);

    pendingPhotoResult = this.cropIfUsingSquareResolution(pendingPhotoResult);

    try {
      const {resolution, blob, timestamp, metadata} =
          await this.checkPhotoResult(pendingPhotoResult);

      metrics.sendCaptureEvent({
        facing: this.getFacing(),
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
        resolutionLevel: this.cameraManager.getPhotoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
      });

      try {
        const name = (new Filenamer(timestamp)).newImageName();
        await this.resultSaver.savePhoto(
            blob, ToteMetricFormat.PHOTO, name, metadata);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.getFacing()});
    } catch (e) {
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  async onPortraitCaptureDone(
      pendingReference: Promise<PhotoResult>,
      pendingPortrait: Promise<PhotoResult>): Promise<void> {
    state.set(PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, true);

    pendingReference = this.cropIfUsingSquareResolution(pendingReference);
    pendingPortrait = this.cropIfUsingSquareResolution(pendingPortrait);

    let hasError = false;
    try {
      const {timestamp, resolution, blob, metadata} =
          await this.checkPhotoResult(pendingReference);

      metrics.sendCaptureEvent({
        facing: this.getFacing(),
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
        resolutionLevel: this.cameraManager.getPhotoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
      });

      // Save reference.
      const filenamer = new Filenamer(timestamp);
      const name = filenamer.newBurstName(false);
      try {
        await this.resultSaver.savePhoto(
            blob, ToteMetricFormat.PHOTO, name, metadata);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }

      try {
        // Save portrait.
        const {blob: portraitBlob, metadata: portraitMetadata} =
            await pendingPortrait;
        const name = filenamer.newBurstName(true);
        await this.resultSaver.savePhoto(
            portraitBlob, ToteMetricFormat.PHOTO, name, portraitMetadata);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_TAKE_PORTRAIT_BOKEH_PHOTO_FAILED);
        // PortraitModeProcessError might be thrown when no face is detected
        // or the segmentataion failed for the scene. Since there is not much
        // we can do for either cases, we tolerate such error.
        if (!(e instanceof PortraitModeProcessError)) {
          throw e;
        }
      }
    } catch (e) {
      hasError = true;
      throw e;
    } finally {
      state.set(
          PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, false,
          {hasError, facing: this.getFacing()});
    }
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  async onDocumentCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    nav.open(ViewName.FLASH);
    let enterInFixMode = false;
    try {
      const {blob, resolution} =
          await this.checkPhotoResult(pendingPhotoResult);
      const helper = ChromeHelper.getInstance();
      let corners = await helper.scanDocumentCorners(blob);
      if (corners === null) {
        corners = getDefaultScanCorners(resolution);
        enterInFixMode = true;
      }
      await this.documentReview.addPage({
        blob,
        corners,
        rotation: Rotation.ANGLE_0,
      });
      metrics.sendCaptureEvent({
        facing: this.getFacing(),
        resolution,
        shutterType: this.shutterType,
        resolutionLevel: this.cameraManager.getPhotoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
      });
    } finally {
      nav.close(ViewName.FLASH);
    }
    await this.reviewDocument(enterInFixMode);
    if (!state.get(state.State.DOC_MODE_REVIEWING)) {
      ChromeHelper.getInstance().maybeTriggerSurvey();
    }
  }

  /**
   * Opens review view to review input blob.
   */
  protected async prepareReview(doReview: () => Promise<void>): Promise<void> {
    // Because the review view will cover the whole camera view, prepare for
    // temporarily turn off camera by stopping preview.
    await this.cameraManager.requestSuspend();
    try {
      await doReview();
    } finally {
      await this.cameraManager.requestResume();
    }
  }

  private async reviewDocument(enterInFixMode = false): Promise<void> {
    await this.prepareReview(async () => {
      const pageCount = await this.documentReview.open({fix: enterInFixMode});
      dom.get('#document-page-count', HTMLDivElement).textContent =
          getI18nMessage(I18nString.NEXT_PAGE_COUNT, pageCount + 1);
      state.set(state.State.DOC_MODE_REVIEWING, pageCount > 0);
    });
  }

  createVideoSaver(): Promise<VideoSaver> {
    return this.resultSaver.startSaveVideo(this.outputVideoRotation);
  }

  playShutterEffect(): void {
    sound.play(dom.get('#sound-shutter', HTMLAudioElement));
    animate.play(this.cameraManager.getPreviewVideo().video);
  }

  private getLowStorageDialogKeys(dialogType: LowStorageDialogType) {
    switch (dialogType) {
      case LowStorageDialogType.AUTO_STOP:
        return {
          title: I18nString.LOW_STORAGE_DIALOG_AUTO_STOP_TITLE,
          description: I18nString.LOW_STORAGE_DIALOG_AUTO_STOP_DESC,
          dialogAction: metrics.LowStorageActionType.SHOW_AUTO_STOP_DIALOG,
          manageAction: metrics.LowStorageActionType.MANAGE_STORAGE_AUTO_STOP,
        };
      case LowStorageDialogType.CANNOT_START:
        return {
          title: I18nString.LOW_STORAGE_DIALOG_CANNOT_START_TITLE,
          description: I18nString.LOW_STORAGE_DIALOG_CANNOT_START_DESC,
          dialogAction: metrics.LowStorageActionType.SHOW_CANNOT_START_DIALOG,
          manageAction:
              metrics.LowStorageActionType.MANAGE_STORAGE_CANNOT_START,
        };
      default:
        assertNotReached();
    }
  }

  private openStorageManagement(): void {
    assert(this.currentLowStorageType !== null);
    const {manageAction} =
        this.getLowStorageDialogKeys(this.currentLowStorageType);
    metrics.sendLowStorageEvent(manageAction);
    ChromeHelper.getInstance().openStorageManagement();
  }

  private showLowStorageDialog(dialogType: LowStorageDialogType): void {
    const {description, dialogAction, title} =
        this.getLowStorageDialogKeys(dialogType);
    this.currentLowStorageType = dialogType;
    metrics.sendLowStorageEvent(dialogAction);
    nav.open(ViewName.LOW_STORAGE_DIALOG, {title, description});
  }

  async onGifCaptureDone({name, gifSaver, resolution, duration}: GifResult):
      Promise<void> {
    nav.open(ViewName.FLASH);

    // Measure the latency of gif encoder finishing rest of the encoding
    // works.
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, true);
    const blob = await gifSaver.endWrite();
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, false);

    const sendEvent = (gifResult: metrics.GifResultType) => {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.GIF,
        facing: this.getFacing(),
        resolution,
        duration,
        shutterType: this.shutterType,
        gifResult,
        resolutionLevel: this.cameraManager.getVideoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
      });
    };

    let result: boolean|null = false;
    await this.prepareReview(async () => {
      await this.review.setReviewPhoto(blob);
      const negative = new review.OptionGroup({
        template: review.ButtonGroupTemplate.NEGATIVE,
        options: [new review.Option(
            {text: I18nString.LABEL_RETAKE}, {exitValue: null})],
      });
      const positive = new review.OptionGroup<boolean>({
        template: review.ButtonGroupTemplate.POSITIVE,
        options: [
          new review.Option(
              {text: I18nString.LABEL_SHARE, icon: 'review_share.svg'}, {
                callback: async () => {
                  sendEvent(metrics.GifResultType.SHARE);
                  await util.share(
                      new File([blob], name, {type: MimeType.GIF}));
                },
              }),
          new review.Option(
              {text: I18nString.LABEL_SAVE, primary: true}, {exitValue: true}),
        ],
      });
      nav.close(ViewName.FLASH);
      result = await this.review.startReview(negative, positive);
    });
    if (result) {
      sendEvent(metrics.GifResultType.SAVE);
      await this.resultSaver.saveGif(blob, name);
    } else {
      sendEvent(metrics.GifResultType.RETAKE);
    }
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  async onVideoCaptureDone(
      {resolution, videoSaver, duration, everPaused, autoStopped}: VideoResult):
      Promise<void> {
    if (autoStopped) {
      this.showLowStorageDialog(LowStorageDialogType.AUTO_STOP);
    }
    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.NORMAL_VIDEO,
        facing: this.getFacing(),
        duration,
        resolution,
        shutterType: this.shutterType,
        everPaused,
        resolutionLevel: this.cameraManager.getVideoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
      });
      await this.resultSaver.finishSaveVideo(videoSaver);
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.getFacing()});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  async onTimeLapseCaptureDone(
      {autoStopped, duration, everPaused, resolution, speed, timeLapseSaver}:
          TimeLapseResult): Promise<void> {
    if (autoStopped) {
      this.showLowStorageDialog(LowStorageDialogType.AUTO_STOP);
    }
    nav.open(ViewName.FLASH);
    state.set(PerfEvent.TIME_LAPSE_CAPTURE_POST_PROCESSING, true);
    try {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.TIME_LAPSE,
        facing: this.getFacing(),
        duration,
        everPaused,
        resolution,
        shutterType: this.shutterType,
        resolutionLevel: this.cameraManager.getVideoResolutionLevel(resolution),
        aspectRatioSet: this.cameraManager.getAspectRatioSet(resolution),
        timeLapseSpeed: speed,
      });
      await this.resultSaver.finishSaveVideo(timeLapseSaver);
      state.set(
          PerfEvent.TIME_LAPSE_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.getFacing()});
    } catch (e) {
      state.set(
          PerfEvent.TIME_LAPSE_CAPTURE_POST_PROCESSING, false,
          {hasError: true});
      throw e;
    } finally {
      nav.close(ViewName.FLASH);
    }
    ChromeHelper.getInstance().maybeTriggerSurvey();
  }

  override layout(): void {
    this.layoutHandler.update();
  }

  override handlingKey(key: util.KeyboardShortcut): boolean {
    if (key === 'Ctrl-Alt-R') {
      toast.showDebugMessage(
          this.cameraManager.getPreviewResolution().toString());
      return true;
    }

    if (state.get(state.State.STREAMING) &&
        !state.get(state.State.ENABLE_SCAN_BARCODE)) {
      if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
          state.get(state.State.TABLET)) {
        if (state.get(state.State.TAKING)) {
          this.endTake();
        } else {
          this.beginTake(metrics.ShutterType.VOLUME_KEY);
        }
        return true;
      }

      if (key === ' ') {
        this.focusShutterButton();
        if (state.get(state.State.TAKING)) {
          this.endTake();
        } else {
          this.beginTake(metrics.ShutterType.KEYBOARD);
        }
        return true;
      }
    }

    return false;
  }

  /**
   * Updates |this.activeDeviceId|.
   */
  private updateActiveCamera(newDeviceId: string|null) {
    // Make the different active camera announced by screen reader.
    if (newDeviceId === this.activeDeviceId) {
      return;
    }
    this.activeDeviceId = newDeviceId;
    if (newDeviceId !== null) {
      const info =
          this.cameraManager.getCameraInfo().getDeviceInfo(newDeviceId);
      speak(I18nString.STATUS_MSG_CAMERA_SWITCHED, info.label);
    }
  }
}
