// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../animation.js';
import {
  assert,
  assertInstanceof,
  assertString,
} from '../assert.js';
import {Camera3DeviceInfo} from '../device/camera3_device_info.js';
import {
  PhotoConstraintsPreferrer,
  VideoConstraintsPreferrer,
} from '../device/constraints_preferrer.js';
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import {StreamConstraints} from '../device/stream_constraints.js';
import * as dom from '../dom.js';
import * as error from '../error.js';
import {Flag} from '../flag.js';
import {Point} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import * as metrics from '../metrics.js';
import {Filenamer} from '../models/file_namer.js';
import * as loadTimeData from '../models/load_time_data.js';
import * as localStorage from '../models/local_storage.js';
import {ResultSaver} from '../models/result_saver.js';
import {VideoSaver} from '../models/video_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as nav from '../nav.js';
import * as newFeatureToast from '../new_feature_toast.js';
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import {speak} from '../spoken_msg.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  ImageBlob,
  MimeType,
  Mode,
  PerfEvent,
  Resolution,
  Rotation,
  ViewName,
} from '../type.js';
import * as util from '../util.js';

import {CameraManager} from './camera/camera_manager.js';
import {Layout} from './camera/layout.js';
import {
  getDefaultScanCorners,
  Modes,
  PhotoHandler,
  PhotoResult,
  ScanHandler,
  setAvc1Parameters,
  Video,
  VideoHandler,
  VideoResult,
} from './camera/mode/index.js';
import {PortraitResult} from './camera/mode/portrait.js';
import {GifResult} from './camera/mode/video.js';
import {Options} from './camera/options.js';
import {Preview} from './camera/preview.js';
import {ScanOptions} from './camera/scan_options.js';
import * as timertick from './camera/timertick.js';
import {VideoEncoderOptions} from './camera/video_encoder_options.js';
import {CropDocument} from './crop_document.js';
import {Dialog} from './dialog.js';
import {PTZPanel} from './ptz_panel.js';
import * as review from './review.js';
import {PrimarySettings} from './settings.js';
import {PTZPanelOptions, View} from './view.js';
import {WarningType} from './warning.js';

/**
 * Thrown when app window suspended during stream reconfiguration.
 */
class CameraSuspendedError extends Error {
  /**
   * @param message Error message.
   */
  constructor(message = 'Camera suspended.') {
    super(message);
    this.name = this.constructor.name;
  }
}

interface ConfigureCandidate {
  deviceId: string;
  mode: Mode;
  captureResolution: Resolution;
  constraints: StreamConstraints;
  videoSnapshotResolution: Resolution;
}

/**
 * Camera-view controller.
 */
export class Camera extends View implements VideoHandler, PhotoHandler,
                                            ScanHandler {
  private readonly cropDocument = new CropDocument();
  private readonly docModeDialogView =
      new Dialog(ViewName.DOCUMENT_MODE_DIALOG);
  private readonly subViews: View[];

  /**
   * Layout handler for the camera view.
   */
  private readonly layoutHandler = new Layout();

  private readonly scanOptions: ScanOptions;

  /**
   * Video preview for the camera.
   */
  private readonly preview: Preview;

  /**
   * Options for the camera.
   */
  private readonly options: Options;

  readonly cameraManager: CameraManager;

  private readonly videoEncoderOptions =
      new VideoEncoderOptions((parameters) => setAvc1Parameters(parameters));

  /**
   * Clock-wise rotation that needs to be applied to the recorded video in
   * order for the video to be replayed in upright orientation.
   */
  private outputVideoRotation = 0;

  /**
   * The preferred device id for camera reconfiguration.
   */
  private deviceId: string|null = null;

  /**
   * Device id of video device of active preview stream. Sets to null when
   * preview become inactive.
   */
  private activeDeviceId: string|null = null;

  /**
   * Modes for the camera.
   */
  private readonly modes: Modes;

  protected readonly review = new review.Review();
  protected facingMode: Facing;
  protected shutterType = metrics.ShutterType.UNKNOWN;
  private retryStartTimeout: number|null = null;

  /**
   * Promise for the camera stream configuration process. It's resolved to
   * boolean for whether the configuration is failed and kick out another
   * round of reconfiguration. Sets to null once the configuration is
   * completed.
   */
  private configuring: Promise<boolean>|null = null;

  /**
   * Promise for the current take of photo or recording.
   */
  private take: Promise<void>|null = null;

  private readonly openPTZPanel = dom.get('#open-ptz-panel', HTMLButtonElement);
  private readonly configureCompleteListeners = new Set<() => void>();

  /**
   * Preview constraints saved for temporarily close/restore preview
   * before/after |ScanHandler| review document result.
   */
  private constraints: StreamConstraints|null = null;

  constructor(
      protected readonly resultSaver: ResultSaver,
      private readonly infoUpdater: DeviceInfoUpdater,
      photoPreferrer: PhotoConstraintsPreferrer,
      videoPreferrer: VideoConstraintsPreferrer,
      protected readonly defaultMode: Mode,
      private readonly perfLogger: PerfLogger,
      facing: Facing|null,
  ) {
    super(ViewName.CAMERA);

    this.subViews = [
      new PrimarySettings(infoUpdater, photoPreferrer, videoPreferrer),
      new PTZPanel(),
      this.review,
      this.cropDocument,
      this.docModeDialogView,
      new View(ViewName.FLASH),
    ];

    this.cameraManager = new CameraManager(() => this.start());

    this.preview = new Preview(this.cameraManager, async () => {
      await this.start();
    });

    this.scanOptions =
        new ScanOptions((point) => this.preview.setPointOfInterest(point));

    this.options = new Options(infoUpdater, () => this.switchCamera());

    this.modes = new Modes(
        this.defaultMode, photoPreferrer, videoPreferrer, () => this.start(),
        this);

    this.facingMode = facing ?? Facing.NOT_SET;

    /**
     * Gets type of ways to trigger shutter from click event.
     */
    const getShutterType = (e: MouseEvent) => {
      if (e.clientX === 0 && e.clientY === 0) {
        return metrics.ShutterType.KEYBOARD;
      }
      return e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents ?
          metrics.ShutterType.TOUCH :
          metrics.ShutterType.MOUSE;
    };

    dom.get('#start-takephoto', HTMLButtonElement)
        .addEventListener('click', (e) => {
          const mouseEvent = assertInstanceof(e, MouseEvent);
          this.beginTake(getShutterType(mouseEvent));
        });

    dom.get('#stop-takephoto', HTMLButtonElement)
        .addEventListener('click', () => this.endTake());

    const videoShutter = dom.get('#recordvideo', HTMLButtonElement);
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake(getShutterType(assertInstanceof(e, MouseEvent)));
      } else {
        this.endTake();
      }
    });

    dom.get('#video-snapshot', HTMLButtonElement)
        .addEventListener('click', () => {
          const videoMode = assertInstanceof(this.modes.current, Video);
          videoMode.takeSnapshot();
        });

    const pauseShutter = dom.get('#pause-recordvideo', HTMLButtonElement);
    pauseShutter.addEventListener('click', () => {
      const videoMode = assertInstanceof(this.modes.current, Video);
      videoMode.togglePaused();
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

    this.initOpenPTZPanel();
  }

  /**
   * Initializes camera view.
   */
  async initialize(): Promise<void> {
    await this.cameraManager.initialize();

    state.addObserver(state.State.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT, () => {
      this.start();
    });
    state.addObserver(state.State.ENABLE_MULTISTREAM_RECORDING, () => {
      this.start();
    });

    this.initVideoEncoderOptions();
    await this.initScanMode();
  }

  private initOpenPTZPanel() {
    this.openPTZPanel.addEventListener('click', () => {
      nav.open(ViewName.PTZ_PANEL, new PTZPanelOptions({
                 stream: this.preview.stream,
                 vidPid: this.preview.getVidPid(),
                 resetPTZ: () => this.preview.resetPTZ(),
               }));
      highlight(false);
    });

    // Highlight effect for PTZ button.
    let toastShown = false;
    const highlight = (enabled) => {
      if (!enabled) {
        if (toastShown) {
          newFeatureToast.hide();
          toastShown = false;
        }
        return;
      }
      toastShown = true;
      newFeatureToast.show(this.openPTZPanel);
      newFeatureToast.focus();
    };

    this.addConfigureCompleteListener(async () => {
      const ptzToastKey = 'isPTZToastShown';
      if (!state.get(state.State.ENABLE_PTZ) ||
          state.get(state.State.IS_NEW_FEATURE_TOAST_SHOWN) ||
          localStorage.getBool(ptzToastKey)) {
        highlight(false);
        return;
      }
      localStorage.set(ptzToastKey, true);
      state.set(state.State.IS_NEW_FEATURE_TOAST_SHOWN, true);
      highlight(true);
    });
  }

  private initVideoEncoderOptions() {
    const options = this.videoEncoderOptions;
    this.addConfigureCompleteListener(() => {
      if (state.get(Mode.VIDEO)) {
        const {width, height, frameRate} =
            this.preview.stream.getVideoTracks()[0].getSettings();
        options.updateValues(new Resolution(width, height), frameRate);
      }
    });
    options.initialize();
  }

  private async initScanMode() {
    const isPlatformSupport =
        await ChromeHelper.getInstance().isDocumentModeSupported();
    state.set(state.State.SHOW_SCAN_MODE, isPlatformSupport);
    if (!isPlatformSupport) {
      return;
    }

    // Check show toast.
    const docModeToastKey = 'isDocModeToastShown';
    if (!state.get(state.State.IS_NEW_FEATURE_TOAST_SHOWN) &&
        !localStorage.getBool(docModeToastKey)) {
      state.set(state.State.IS_NEW_FEATURE_TOAST_SHOWN, true);
      localStorage.set(docModeToastKey, true);
      // aria-owns don't work on HTMLInputElement, show toast on parent div
      // instead.
      const scanModeBtn = dom.get('input[data-mode="scan"]', HTMLInputElement);
      const scanModeItem =
          assertInstanceof(scanModeBtn.parentElement, HTMLDivElement);
      newFeatureToast.show(scanModeItem);
      scanModeBtn.addEventListener('click', () => {
        newFeatureToast.hide();
      });
    }

    const docModeDialogKey = 'isDocModeDialogShown';
    if (!localStorage.getBool(docModeDialogKey)) {
      const checkShowDialog = () => {
        if (!state.get(Mode.SCAN) ||
            !this.scanOptions.isDocumentModeEanbled()) {
          return;
        }
        this.removeConfigureCompleteListener(checkShowDialog);
        localStorage.set(docModeDialogKey, true);
        const message = loadTimeData.getI18nMessage(
            I18nString.DOCUMENT_MODE_DIALOG_INTRO_TITLE);
        nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
      };
      this.addConfigureCompleteListener(checkShowDialog);
    }

    // When entering document mode, refocus to shutter button for letting user
    // to take document photo with space key as shortcut. See b/196907822.
    const checkRefocus = () => {
      if (!state.get(state.State.CAMERA_CONFIGURING) && state.get(Mode.SCAN) &&
          this.scanOptions.isDocumentModeEanbled() &&
          nav.isTopMostView(this.name)) {
        dom.getAll('button.shutter', HTMLButtonElement)
            .forEach((btn) => btn.offsetParent && btn.focus());
      }
    };
    state.addObserver(state.State.CAMERA_CONFIGURING, checkRefocus);
    this.scanOptions.onChange = checkRefocus;
  }

  private addConfigureCompleteListener(listener: () => void) {
    this.configureCompleteListeners.add(listener);
  }

  private removeConfigureCompleteListener(listener: () => void) {
    this.configureCompleteListeners.delete(listener);
  }

  getSubViews(): View[] {
    return this.subViews;
  }

  focus(): void {
    (async () => {
      await this.configuring;

      // Check the view is still on the top after await.
      if (!nav.isTopMostView(ViewName.CAMERA)) {
        return;
      }

      if (newFeatureToast.isShowing()) {
        newFeatureToast.focus();
        return;
      }

      // Avoid focusing invisible shutters.
      dom.getAll('button.shutter', HTMLButtonElement)
          .forEach((btn) => btn.offsetParent && btn.focus());
    })();
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
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
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take = (async () => {
      let hasError = false;
      try {
        // Record and keep the rotation only at the instance the user starts the
        // capture. Users may change the device orientation while taking video.
        const cameraFrameRotation = await (async () => {
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return 0;
          }
          assert(this.activeDeviceId !== null);
          return await deviceOperator.getCameraFrameRotation(
              this.activeDeviceId);
        })();
        // Translate the camera frame rotation back to the UI rotation, which is
        // what we need to rotate the captured video with.
        this.outputVideoRotation = (360 - cameraFrameRotation) % 360;
        await timertick.start();
        const captureDone = await this.modes.current.startCapture();
        await captureDone();
      } catch (e) {
        hasError = true;
        if (e instanceof CanceledError) {
          return;
        }
        error.reportError(
            ErrorType.START_CAPTURE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
      } finally {
        this.take = null;
        state.set(
            state.State.TAKING, false, {hasError, facing: this.facingMode});
        this.focus();  // Refocus the visible shutter button for ChromeVox.
      }
    })();
    return this.take;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any.)
   * @return Promise for the operation.
   */
  private endTake(): Promise<void> {
    timertick.cancel();
    this.modes.current.stopCapture();
    return Promise.resolve(this.take);
  }

  getPreviewAspectRatio(): number {
    const {videoWidth, videoHeight} = this.preview.getVideoElement();
    return videoWidth / videoHeight;
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

  async handleVideoSnapshot({resolution, blob, timestamp}: PhotoResult):
      Promise<void> {
    metrics.sendCaptureEvent({
      facing: this.facingMode,
      resolution,
      shutterType: this.shutterType,
      isVideoSnapshot: true,
    });
    try {
      const name = (new Filenamer(timestamp)).newImageName();
      await this.resultSaver.savePhoto(blob, name, /* metadata */ null);
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

  async onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);
    try {
      const {resolution, blob, timestamp, metadata} =
          await this.checkPhotoResult(pendingPhotoResult);

      metrics.sendCaptureEvent({
        facing: this.facingMode,
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
      });

      try {
        const name = (new Filenamer(timestamp)).newImageName();
        await this.resultSaver.savePhoto(blob, name, metadata);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facingMode});
    } catch (e) {
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  async onPortraitCaptureDone(pendingPortraitResult: Promise<PortraitResult>):
      Promise<void> {
    state.set(PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, true);
    let hasError = false;
    try {
      const {timestamp, resolution, blob, metadata, pendingPortrait} =
          await this.checkPhotoResult(pendingPortraitResult);
      const portraitBlobAndMetadata =
          await this.checkPhotoResult(pendingPortrait);

      metrics.sendCaptureEvent({
        facing: this.facingMode,
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
      });

      try {
        // Save reference.
        const filenamer = new Filenamer(timestamp);
        const name = filenamer.newBurstName(false);
        await this.resultSaver.savePhoto(blob, name, metadata);

        // Save portrait.
        if (portraitBlobAndMetadata !== null) {
          const {blob, metadata} = portraitBlobAndMetadata;
          const name = filenamer.newBurstName(true);
          await this.resultSaver.savePhoto(blob, name, metadata);
        } else {
          toast.show(I18nString.ERROR_MSG_TAKE_PORTRAIT_BOKEH_PHOTO_FAILED);
        }
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }
    } catch (e) {
      hasError = true;
      throw e;
    } finally {
      state.set(
          PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, false,
          {hasError, facing: this.facingMode});
    }
  }

  async onDocumentCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    const {blob: rawBlob, resolution, timestamp, metadata} =
        await this.checkPhotoResult(pendingPhotoResult);
    const helper = ChromeHelper.getInstance();
    const corners = await helper.scanDocumentCorners(rawBlob);
    const reviewResult =
        await this.reviewDocument({blob: rawBlob, resolution}, corners);
    if (reviewResult === null) {
      throw new CanceledError('Cancelled after review document');
    }
    const {docBlob, mimeType} = reviewResult;
    let blob = docBlob;
    if (mimeType === MimeType.PDF) {
      blob = await helper.convertToPdf(blob);
    }
    try {
      const name = (new Filenamer(timestamp)).newDocumentName(mimeType);
      await this.resultSaver.savePhoto(blob, name, metadata);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * Opens review view to review input blob.
   */
  protected async prepareReview(doReview: () => Promise<void>): Promise<void> {
    // Because the review view will cover the whole camera view, prepare for
    // temporarily turn off camera by stopping preview.
    this.constraints = this.preview.getConstraints();
    await this.preview.close();
    await this.scanOptions.detachPreview();
    try {
      await doReview();
    } finally {
      assert(this.constraints !== null);
      await this.modes.prepareDevice();
      await this.preview.open(this.constraints);
      this.modes.current.updatePreview(this.preview.getVideo());
      await this.scanOptions.attachPreview(this.preview.getVideoElement());
    }
  }

  /**
   * @param originImage Original photo to be cropped document from.
   * @param refCorners Initial reference document corner positions detected by
   *     scan API. Sets to null if scan API cannot find any reference corner
   *     from |rawBlob|.
   * @return Returns the processed document blob and which mime type user
   *     choose to save. Null for cancel document.
   */
  private async reviewDocument(
      originImage: ImageBlob, refCorners: Point[]|null):
      Promise<{docBlob: Blob, mimeType: MimeType}|null> {
    const needFirstRecrop = refCorners === null;
    const allowRecrop = loadTimeData.getChromeFlag(Flag.DOCUMENT_MANUAL_CROP);
    if (needFirstRecrop && !allowRecrop) {
      const message = loadTimeData.getI18nMessage(
          I18nString.DOCUMENT_MODE_DIALOG_NOT_DETECTED_TITLE);
      nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
      throw new CanceledError(`Couldn't detect a document`);
    }

    nav.open(ViewName.FLASH);
    const helper = ChromeHelper.getInstance();
    let result = null;
    try {
      await this.prepareReview(async () => {
        const doCrop = (blob, corners, rotation) =>
            helper.convertToDocument(blob, corners, rotation, MimeType.JPEG);
        let corners =
            refCorners || getDefaultScanCorners(originImage.resolution);
        let docBlob;
        let fixType = metrics.DocFixType.NONE;
        const sendEvent = (docResult) => {
          metrics.sendCaptureEvent({
            facing: this.facingMode,
            resolution: originImage.resolution,
            shutterType: this.shutterType,
            docResult,
            docFixType: fixType,
          });
        };

        const doRecrop = async () => {
          const {corners: newCorners, rotation} =
              await this.cropDocument.reviewCropArea(corners);

          fixType = (() => {
            const isFixRotation = rotation !== Rotation.ANGLE_0;
            const isFixPosition = newCorners.some(({x, y}, idx) => {
              const {x: oldX, y: oldY} = corners[idx];
              return Math.abs(x - oldX) * originImage.resolution.width > 1 ||
                  Math.abs(y - oldY) * originImage.resolution.height > 1;
            });
            if (isFixRotation && isFixPosition) {
              return metrics.DocFixType.FIX_BOTH;
            }
            if (isFixRotation) {
              return metrics.DocFixType.FIX_ROTATION;
            }
            if (isFixPosition) {
              return metrics.DocFixType.FIX_POSITION;
            }
            return metrics.DocFixType.NO_FIX;
          })();

          corners = newCorners;
          docBlob = await (async () => {
            nav.open(ViewName.FLASH);
            try {
              return await doCrop(originImage.blob, corners, rotation);
            } finally {
              nav.close(ViewName.FLASH);
            }
          })();
          await this.review.setReviewPhoto(docBlob);
        };

        await this.cropDocument.setReviewPhoto(originImage.blob);
        if (needFirstRecrop) {
          nav.close(ViewName.FLASH);
          await doRecrop();
        } else {
          docBlob = await doCrop(originImage.blob, corners, Rotation.ANGLE_0);
          await this.review.setReviewPhoto(docBlob);
          nav.close(ViewName.FLASH);
        }

        const positive = new review.OptionGroup({
          template: review.ButtonGroupTemplate.positive,
          options: [
            new review.Option({text: I18nString.LABEL_SAVE_PDF_DOCUMENT}, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PDF);
              },
              exitValue: MimeType.PDF,
            }),
            new review.Option({text: I18nString.LABEL_SAVE_PHOTO_DOCUMENT}, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PHOTO);
              },
              exitValue: MimeType.JPEG,
            }),
            new review.Option({text: I18nString.LABEL_SHARE}, {
              callback: async () => {
                sendEvent(metrics.DocResultType.SHARE);
                const type = MimeType.JPEG;
                const name = (new Filenamer()).newDocumentName(type);
                await util.share(new File([docBlob], name, {type}));
              },
            }),
          ],
        });

        const negOptions = [
          new review.Option({text: I18nString.LABEL_RETAKE}, {
            callback: () => {
              sendEvent(metrics.DocResultType.CANCELED);
            },
            exitValue: null,
          }),
        ];
        if (allowRecrop) {
          negOptions.unshift(
              new review.Option({text: I18nString.LABEL_FIX_DOCUMENT}, {
                callback: doRecrop,
                hasPopup: true,
              }));
        }
        const negative = new review.OptionGroup({
          template: review.ButtonGroupTemplate.negative,
          options: negOptions,
        });

        const mimeType = await this.review.startReview(positive, negative);
        assert(mimeType !== undefined);
        if (mimeType !== null) {
          result = {docBlob, mimeType};
        }
      });
    } finally {
      nav.close(ViewName.FLASH);
    }
    return result;
  }

  createVideoSaver(): Promise<VideoSaver> {
    return this.resultSaver.startSaveVideo(this.outputVideoRotation);
  }

  getPreviewVideo(): HTMLVideoElement {
    const video = this.preview.getVideoElement();
    assertInstanceof(video, HTMLVideoElement);
    return video;
  }

  playShutterEffect(): void {
    sound.play(dom.get('#sound-shutter', HTMLAudioElement));
    animate.play(this.preview.getVideoElement());
  }

  async onGifCaptureDone({name, gifSaver, resolution, duration}: GifResult):
      Promise<void> {
    nav.open(ViewName.FLASH);

    // Measure the latency of gif encoder finishing rest of the encoding
    // works.
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, true);
    const blob = await gifSaver.endWrite();
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, false);

    const sendEvent = (gifResult) => {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.GIF,
        facing: this.facingMode,
        resolution,
        duration,
        shutterType: this.shutterType,
        gifResult,
      });
    };

    let result = false;
    await this.prepareReview(async () => {
      await this.review.setReviewPhoto(blob);
      const positive = new review.OptionGroup({
        template: review.ButtonGroupTemplate.positive,
        options: [
          new review.Option({text: I18nString.LABEL_SAVE}, {exitValue: true}),
          new review.Option({text: I18nString.LABEL_SHARE}, {
            callback: async () => {
              sendEvent(metrics.GifResultType.SHARE);
              await util.share(new File([blob], name, {type: MimeType.GIF}));
            },
          }),
        ],
      });
      const negative = new review.OptionGroup({
        template: review.ButtonGroupTemplate.negative,
        options: [new review.Option(
            {text: I18nString.LABEL_RETAKE}, {exitValue: null})],
      });
      nav.close(ViewName.FLASH);
      result = (await this.review.startReview(positive, negative)) as boolean;
    });
    if (result) {
      sendEvent(metrics.GifResultType.SAVE);
      await this.resultSaver.saveGif(blob, name);
    } else {
      sendEvent(metrics.GifResultType.RETAKE);
    }
  }

  async onVideoCaptureDone({resolution, videoSaver, duration, everPaused}:
                               VideoResult): Promise<void> {
    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.NORMAL_VIDEO,
        facing: this.facingMode,
        duration,
        resolution,
        shutterType: this.shutterType,
        everPaused,
      });
      await this.resultSaver.finishSaveVideo(videoSaver);
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facingMode});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  layout(): void {
    this.layoutHandler.update();
  }

  handlingKey(key: string): boolean {
    if (key === 'Ctrl-R') {
      toast.showDebugMessage(this.preview.toString());
      return true;
    }
    if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
        state.get(state.State.TABLET) && state.get(state.State.STREAMING)) {
      if (state.get(state.State.TAKING)) {
        this.endTake();
      } else {
        this.beginTake(metrics.ShutterType.VOLUME_KEY);
      }
      return true;
    }
    return false;
  }

  /**
   * Switches to the next available camera device.
   */
  private switchCamera(): Promise<void>|null {
    if (state.get(PerfEvent.CAMERA_SWITCHING) ||
        state.get(state.State.CAMERA_CONFIGURING) ||
        !state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return null;
    }
    state.set(PerfEvent.CAMERA_SWITCHING, true);
    const devices = this.infoUpdater.getDevicesInfo();
    let index = devices.findIndex((entry) => entry.deviceId === this.deviceId);
    if (index === -1) {
      index = 0;
    }
    if (devices.length > 0) {
      index = (index + 1) % devices.length;
      this.deviceId = devices[index].deviceId;
    }
    return (async () => {
      const isSuccess = await this.start();
      state.set(PerfEvent.CAMERA_SWITCHING, false, {hasError: !isSuccess});
    })();
  }

  protected async getModeCandidates(deviceId: string|null): Promise<Mode[]> {
    const supportedModes = await this.modes.getSupportedModes(deviceId);
    return this.modes.getModeCandidates().filter(
        (m) => supportedModes.includes(m));
  }

  /**
   * Gets the video device ids sorted by preference.
   */
  private getDeviceIdCandidates(): string[] {
    let devices: Array<Camera3DeviceInfo|MediaDeviceInfo>;
    /**
     * Object mapping from device id to facing. Set to null for fake cameras.
     */
    let facings: Record<string, Facing>|null = null;

    const camera3Info = this.infoUpdater.getCamera3DevicesInfo();
    if (camera3Info) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
    } else {
      devices = this.infoUpdater.getDevicesInfo();
    }

    const preferredFacing = this.facingMode === Facing.NOT_SET ?
        util.getDefaultFacing() :
        this.facingMode;
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.deviceId ? a === this.deviceId :
                          (facings && facings[a] === preferredFacing)) {
        return -1;
      }
      return 1;
    });
    return sorted;
  }

  private async *
      getConfigurationCandidates(): AsyncIterable<ConfigureCandidate> {
    const deviceOperator = await DeviceOperator.getInstance();

    for (const deviceId of this.getDeviceIdCandidates()) {
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
   * Stops camera and tries to start camera stream again if possible.
   * @return Promise resolved to whether start camera successfully.
   */
  async start(): Promise<boolean> {
    // To prevent multiple callers enter this function at the same time, wait
    // until previous caller resets configuring to null.
    while (this.configuring !== null) {
      if (!await this.configuring) {
        // Retry will be kicked out soon.
        return false;
      }
    }
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.configuring = (async () => {
      try {
        if (state.get(state.State.TAKING)) {
          await this.endTake();
        }
      } finally {
        await this.stopStreams();
      }
      return this.startInternal();
    })();
    return this.configuring;
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

  /**
   * Updates |this.activeDeviceId|.
   */
  private updateActiveCamera(newDeviceId: string) {
    // Make the different active camera announced by screen reader.
    if (newDeviceId === this.activeDeviceId) {
      return;
    }
    this.activeDeviceId = newDeviceId;
    const info = this.infoUpdater.getDeviceInfo(newDeviceId);
    if (info !== null) {
      speak(I18nString.STATUS_MSG_CAMERA_SWITCHED, info.label);
    }
  }

  /**
   * Starts camera configuration process.
   * @return Resolved to boolean for whether the configuration is succeeded or
   *     kicks out another round of reconfiguration.
   */
  private async startInternal(): Promise<boolean> {
    try {
      await this.infoUpdater.lockDeviceInfo(async () => {
        if (this.cameraManager.shouldSuspended()) {
          throw new CameraSuspendedError();
        }
        const congfigureSucceed = await (async () => {
          const deviceOperator = await DeviceOperator.getInstance();
          state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);

          for await (const c of this.getConfigurationCandidates()) {
            if (this.cameraManager.shouldSuspended()) {
              throw new CameraSuspendedError();
            }
            this.modes.setCaptureParams(
                c.mode, c.constraints, c.captureResolution,
                c.videoSnapshotResolution);
            try {
              await this.modes.prepareDevice();
              const factory = this.modes.getModeFactory(c.mode);
              const stream = await this.preview.open(c.constraints);
              this.facingMode = this.preview.getFacing();
              const currentDeviceId = assertString(this.preview.getDeviceId());

              await this.checkEnablePTZ(c);
              this.options.updateValues(
                  stream, currentDeviceId, this.facingMode);
              factory.setPreviewVideo(this.preview.getVideo());
              factory.setFacing(this.facingMode);
              await this.modes.updateModeSelectionUI(c.deviceId);
              await this.modes.updateMode(
                  factory, stream, this.facingMode, c.deviceId);
              await this.scanOptions.attachPreview(
                  this.preview.getVideoElement());
              for (const l of this.configureCompleteListeners) {
                l();
              }
              nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
              this.updateActiveCamera(currentDeviceId);

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
        })();

        if (!congfigureSucceed) {
          throw new CameraSuspendedError();
        }
      });
      this.configuring = null;
      state.set(state.State.CAMERA_CONFIGURING, false);

      return true;
    } catch (e) {
      this.activeDeviceId = null;
      if (!(e instanceof CameraSuspendedError)) {
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
        nav.open(ViewName.WARNING, WarningType.NO_CAMERA);
      }
      // Schedule to retry.
      if (this.retryStartTimeout) {
        clearTimeout(this.retryStartTimeout);
        this.retryStartTimeout = null;
      }
      this.retryStartTimeout = setTimeout(() => {
        this.configuring = this.startInternal();
      }, 100);

      this.perfLogger.interrupt();
      return false;
    }
  }

  /**
   * Stop extra stream and preview stream.
   */
  private async stopStreams() {
    await this.modes.clear();
    await this.preview.close();
    await this.scanOptions.detachPreview();
  }
}
