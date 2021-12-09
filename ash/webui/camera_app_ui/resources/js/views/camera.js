// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../animation.js';
import {
  assert,
  assertInstanceof,
} from '../assert.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from '../device/stream_constraints.js';
import * as dom from '../dom.js';
import * as error from '../error.js';
import {Flag} from '../flag.js';
import {I18nString} from '../i18n_string.js';
import * as metrics from '../metrics.js';
import {Filenamer} from '../models/file_namer.js';
import * as loadTimeData from '../models/load_time_data.js';
import * as localStorage from '../models/local_storage.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from '../models/result_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {ScreenState} from '../mojo/type.js';
import * as nav from '../nav.js';
import * as newFeatureToast from '../new_feature_toast.js';
// eslint-disable-next-line no-unused-vars
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  MimeType,
  Mode,
  Resolution,
  Rotation,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {windowController} from '../window_controller.js';

import {Layout} from './camera/layout.js';
import {
  getDefaultScanCorners,
  Modes,
  PhotoHandler,  // eslint-disable-line no-unused-vars
  ScanHandler,   // eslint-disable-line no-unused-vars
  setAvc1Parameters,
  Video,
  VideoHandler,  // eslint-disable-line no-unused-vars
} from './camera/mode/index.js';
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
   * @param {string=} message Error message.
   */
  constructor(message = 'Camera suspended.') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Camera-view controller.
 * @implements {VideoHandler}
 * @implements {PhotoHandler}
 * @implements {ScanHandler}
 */
export class Camera extends View {
  /**
   * @param {!ResultSaver} resultSaver
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {!Mode} defaultMode
   * @param {!PerfLogger} perfLogger
   * @param {?Facing} facing
   */
  constructor(
      resultSaver, infoUpdater, photoPreferrer, videoPreferrer, defaultMode,
      perfLogger, facing) {
    super(ViewName.CAMERA);

    /**
     * @type {!DeviceInfoUpdater}
     * @private
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {!Mode}
     * @protected
     */
    this.defaultMode_ = defaultMode;

    /**
     * @type {!PerfLogger}
     * @private
     */
    this.perfLogger_ = perfLogger;

    /**
     * @type {!review.Review}
     * @private
     */
    this.review_ = new review.Review();

    /**
     * @type {!CropDocument}
     * @private
     */
    this.cropDocument_ = new CropDocument();

    /**
     * @type {!Dialog}
     * @private
     */
    this.docModeDialogView_ = new Dialog(ViewName.DOCUMENT_MODE_DIALOG);

    /**
     * @const {!Array<!View>}
     * @private
     */
    this.subViews_ = [
      new PrimarySettings(infoUpdater, photoPreferrer, videoPreferrer),
      new PTZPanel(),
      this.review_,
      this.cropDocument_,
      this.docModeDialogView_,
      new View(ViewName.FLASH),
    ];

    /**
     * Layout handler for the camera view.
     * @type {!Layout}
     * @private
     */
    this.layout_ = new Layout();

    /**
     * @type {!ScanOptions}
     * @private
     */
    this.scanOptions_ = new ScanOptions({
      doReconfigure: () => this.start(),
      infoUpdater: this.infoUpdater_,
    });

    /**
     * Video preview for the camera.
     * @type {!Preview}
     * @private
     */
    this.preview_ = new Preview(() => this.start());

    /**
     * Options for the camera.
     * @type {!Options}
     * @private
     */
    this.options_ = new Options(infoUpdater, () => this.start());

    /**
     * @type {!VideoEncoderOptions}
     * @private
     */
    this.videoEncoderOptions_ =
        new VideoEncoderOptions((parameters) => setAvc1Parameters(parameters));

    /**
     * Clock-wise rotation that needs to be applied to the recorded video in
     * order for the video to be replayed in upright orientation.
     * @type {number}
     * @private
     */
    this.outputVideoRotation_ = 0;

    /**
     * @type {!ResultSaver}
     * @protected
     */
    this.resultSaver_ = resultSaver;

    /**
     * Device id of video device of active preview stream. Sets to null when
     * preview become inactive.
     * @type {?string}
     * @private
     */
    this.activeDeviceId_ = null;

    /**
     * The last time of all screen state turning from OFF to ON during the app
     * execution. Sets to -Infinity for no such time since app is opened.
     * @type {number}
     * @private
     */
    this.lastScreenOnTime_ = -Infinity;

    /**
     * Modes for the camera.
     * @type {!Modes}
     * @private
     */
    this.modes_ = new Modes(
        this.defaultMode_, photoPreferrer, videoPreferrer, () => this.start(),
        this, this, this);

    /**
     * @type {!Facing}
     * @protected
     */
    this.facingMode_ = facing ?? Facing.NOT_SET;

    /**
     * @type {!metrics.ShutterType}
     * @protected
     */
    this.shutterType_ = metrics.ShutterType.UNKNOWN;

    /**
     * @type {boolean}
     * @private
     */
    this.locked_ = false;

    /**
     * @type {?number}
     * @private
     */
    this.retryStartTimeout_ = null;

    /**
     * Promise for the camera stream configuration process. It's resolved to
     * boolean for whether the configuration is failed and kick out another
     * round of reconfiguration. Sets to null once the configuration is
     * completed.
     * @type {?Promise<boolean>}
     * @private
     */
    this.configuring_ = null;

    /**
     * Promise for the current take of photo or recording.
     * @type {?Promise}
     * @protected
     */
    this.take_ = null;

    /**
     * @type {!HTMLButtonElement}
     */
    this.openPTZPanel_ = dom.get('#open-ptz-panel', HTMLButtonElement);

    /**
     * @const {!Set<function(): *>}
     * @private
     */
    this.configureCompleteListener_ = new Set();

    /**
     * Preview constraints saved for temporarily close/restore preview
     * before/after |ScanHandler| review document result.
     * @type {?StreamConstraints}
     * @private
     */
    this.constraints_ = null;

    /**
     * Gets type of ways to trigger shutter from click event.
     * @param {!MouseEvent} e
     * @return {!metrics.ShutterType}
     */
    const getShutterType = (e) => {
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
        .addEventListener('click', () => this.endTake_());

    const videoShutter = dom.get('#recordvideo', HTMLButtonElement);
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake(getShutterType(assertInstanceof(e, MouseEvent)));
      } else {
        this.endTake_();
      }
    });

    dom.get('#video-snapshot', HTMLButtonElement)
        .addEventListener('click', () => {
          const videoMode = assertInstanceof(this.modes_.current, Video);
          videoMode.takeSnapshot();
        });

    const pauseShutter = dom.get('#pause-recordvideo', HTMLButtonElement);
    pauseShutter.addEventListener('click', () => {
      const videoMode = assertInstanceof(this.modes_.current, Video);
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

    this.initOpenPTZPanel_();

    // Monitor the states to stop camera when locked/minimized.
    const idleDetector = new IdleDetector();
    idleDetector.addEventListener('change', () => {
      this.locked_ = idleDetector.screenState === 'locked';
      if (this.locked_) {
        this.start();
      }
    });
    idleDetector.start().catch((e) => {
      error.reportError(
          ErrorType.IDLE_DETECTOR_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    });

    document.addEventListener('visibilitychange', () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (this.isTabletBackground_() && !recording) {
        this.start();
      }
    });
  }

  /**
   * Initializes camera view.
   * @return {!Promise}
   */
  async initialize() {
    const helper = await ChromeHelper.getInstance();

    const setTablet = (isTablet) => state.set(state.State.TABLET, isTablet);
    const isTablet = await helper.initTabletModeMonitor(setTablet);
    setTablet(isTablet);

    const setScreenOffAuto = (s) => {
      const offAuto = s === ScreenState.OFF_AUTO;
      state.set(state.State.SCREEN_OFF_AUTO, offAuto);
    };
    const screenState = await helper.initScreenStateMonitor(setScreenOffAuto);
    setScreenOffAuto(screenState);

    const updateExternalScreen = (hasExternalScreen) => {
      state.set(state.State.HAS_EXTERNAL_SCREEN, hasExternalScreen);
    };
    const hasExternalScreen =
        await helper.initExternalScreenMonitor(updateExternalScreen);
    updateExternalScreen(hasExternalScreen);

    const handleScreenStateChange = () => {
      if (this.screenOff_) {
        this.start();
      } else {
        this.lastScreenOnTime_ = performance.now();
      }
    };

    state.addObserver(state.State.SCREEN_OFF_AUTO, handleScreenStateChange);
    state.addObserver(state.State.HAS_EXTERNAL_SCREEN, handleScreenStateChange);

    state.addObserver(state.State.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT, () => {
      this.start();
    });
    state.addObserver(state.State.ENABLE_MULTISTREAM_RECORDING, () => {
      this.start();
    });

    this.initVideoEncoderOptions_();
    await this.initScanMode_();
  }

  /**
   * @private
   */
  initOpenPTZPanel_() {
    this.openPTZPanel_.addEventListener('click', () => {
      nav.open(ViewName.PTZ_PANEL, new PTZPanelOptions({
                 stream: this.preview_.stream,
                 vidPid: this.preview_.getVidPid(),
                 resetPTZ: () => this.preview_.resetPTZ(),
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
      newFeatureToast.show(this.openPTZPanel_);
      newFeatureToast.focus();
    };

    this.addConfigureCompleteListener_(async () => {
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

  /**
   * @private
   */
  initVideoEncoderOptions_() {
    const options = this.videoEncoderOptions_;
    this.addConfigureCompleteListener_(() => {
      if (state.get(Mode.VIDEO)) {
        const {width, height, frameRate} =
            this.preview_.stream.getVideoTracks()[0].getSettings();
        options.updateValues(new Resolution(width, height), frameRate);
      }
    });
    options.initialize();
  }

  /**
   * @private
   */
  async initScanMode_() {
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
            !this.scanOptions_.isDocumentModeEanbled()) {
          return;
        }
        this.removeConfigureCompleteListener_(checkShowDialog);
        localStorage.set(docModeDialogKey, true);
        const message = loadTimeData.getI18nMessage(
            I18nString.DOCUMENT_MODE_DIALOG_INTRO_TITLE);
        nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
      };
      this.addConfigureCompleteListener_(checkShowDialog);
    }

    // When entering document mode, refocus to shutter button for letting user
    // to take document photo with space key as shortcut. See b/196907822.
    const checkRefocus = () => {
      if (!state.get(state.State.CAMERA_CONFIGURING) && state.get(Mode.SCAN) &&
          this.scanOptions_.isDocumentModeEanbled() &&
          nav.isTopMostView(this.name)) {
        dom.getAll('button.shutter', HTMLButtonElement)
            .forEach((btn) => btn.offsetParent && btn.focus());
      }
    };
    state.addObserver(state.State.CAMERA_CONFIGURING, checkRefocus);
    this.scanOptions_.onChange = checkRefocus;
  }

  /**
   * @param {function(): *} listener
   * @private
   */
  addConfigureCompleteListener_(listener) {
    this.configureCompleteListener_.add(listener);
  }

  /**
   * @param {function(): *} listener
   * @private
   */
  removeConfigureCompleteListener_(listener) {
    this.configureCompleteListener_.delete(listener);
  }

  /**
   * @return {boolean} If the App window is invisible to user with respect to
   * screen off state.
   * @private
   */
  get screenOff_() {
    return state.get(state.State.SCREEN_OFF_AUTO) &&
        !state.get(state.State.HAS_EXTERNAL_SCREEN);
  }

  /**
   * @return {boolean} Returns if window is fully overlapped by other window in
   * both window mode or tablet mode.
   * @private
   */
  get isVisible_() {
    return document.visibilityState !== 'hidden';
  }

  /**
   * @return {boolean} Whether window is put to background in tablet mode.
   * @private
   */
  isTabletBackground_() {
    return state.get(state.State.TABLET) && !this.isVisible_;
  }

  /**
   * Whether app window is suspended.
   * @return {boolean}
   */
  isSuspended() {
    return this.locked_ || windowController.isMinimized() ||
        state.get(state.State.SUSPEND) || this.screenOff_ ||
        this.isTabletBackground_();
  }

  /**
   * @override
   */
  getSubViews() {
    return this.subViews_;
  }

  /**
   * @override
   */
  focus() {
    (async () => {
      await this.configuring_;

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
   * @param {!metrics.ShutterType} shutterType The shutter is triggered by which
   *     shutter type.
   * @return {?Promise} Promise resolved when take action completes. Returns
   *     null if CCA can't start take action.
   */
  beginTake(shutterType) {
    if (state.get(state.State.CAMERA_CONFIGURING) ||
        state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.shutterType_ = shutterType;
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take_ = (async () => {
      let hasError = false;
      try {
        // Record and keep the rotation only at the instance the user starts the
        // capture. Users may change the device orientation while taking video.
        const cameraFrameRotation = await (async () => {
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return 0;
          }
          assert(this.activeDeviceId_ !== null);
          return await deviceOperator.getCameraFrameRotation(
              this.activeDeviceId_);
        })();
        // Translate the camera frame rotation back to the UI rotation, which is
        // what we need to rotate the captured video with.
        this.outputVideoRotation_ = (360 - cameraFrameRotation) % 360;
        await timertick.start();
        await this.modes_.current.startCapture();
      } catch (e) {
        hasError = true;
        if (e instanceof CanceledError) {
          return;
        }
        error.reportError(
            ErrorType.START_CAPTURE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
      } finally {
        this.take_ = null;
        state.set(
            state.State.TAKING, false, {hasError, facing: this.facingMode_});
        this.focus();  // Refocus the visible shutter button for ChromeVox.
      }
    })();
    return this.take_;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any.)
   * @return {!Promise} Promise for the operation.
   * @private
   */
  endTake_() {
    timertick.cancel();
    this.modes_.current.stopCapture();
    return Promise.resolve(this.take_);
  }

  /**
   * @return {number}
   */
  getPreviewAspectRatio() {
    const {videoWidth, videoHeight} = this.preview_.getVideoElement();
    return videoWidth / videoHeight;
  }

  /**
   * @override
   */
  async handleResultPhoto({resolution, blob, isVideoSnapshot}, name) {
    metrics.sendCaptureEvent({
      facing: this.facingMode_,
      resolution,
      shutterType: this.shutterType_,
      isVideoSnapshot,
    });
    try {
      await this.resultSaver_.savePhoto(blob, name);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * @override
   */
  async handleResultDocument({blob, resolution, mimeType}, name) {
    try {
      await this.resultSaver_.savePhoto(blob, name);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * Opens review view to review input blob.
   * @param {function(): !Promise} doReview
   * @return {!Promise}
   * @private
   */
  async prepareReview_(doReview) {
    // Because the review view will cover the whole camera view, prepare for
    // temporarily turn off camera by stopping preview.
    this.constraints_ = this.preview_.getConstraints();
    await this.preview_.close();
    await this.scanOptions_.detachPreview();
    try {
      await doReview();
    } finally {
      assert(this.constraints_ !== null);
      await this.modes_.prepareDevice();
      await this.preview_.open(this.constraints_);
      this.modes_.current.updatePreview(this.preview_.stream);
      await this.scanOptions_.attachPreview(this.preview_.getVideoElement());
    }
  }

  /**
   * @override
   */
  async reviewDocument(originImage, refCorners) {
    const needFirstRecrop = refCorners === null;
    const allowRecrop = loadTimeData.getChromeFlag(Flag.DOCUMENT_MANUAL_CROP);
    if (needFirstRecrop && !allowRecrop) {
      const message = loadTimeData.getI18nMessage(
          I18nString.DOCUMENT_MODE_DIALOG_NOT_DETECTED_TITLE);
      nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
      throw new CanceledError(`Couldn't detect a document`);
    }

    nav.open(ViewName.FLASH);
    const helper = await ChromeHelper.getInstance();
    let result = null;
    try {
      await this.prepareReview_(async () => {
        const doCrop = (blob, corners, rotation) =>
            helper.convertToDocument(blob, corners, rotation, MimeType.JPEG);
        let corners =
            refCorners || getDefaultScanCorners(originImage.resolution);
        let docBlob;
        let fixType = metrics.DocFixType.NONE;
        const sendEvent = (docResult) => {
          metrics.sendCaptureEvent({
            facing: this.facingMode_,
            resolution: originImage.resolution,
            shutterType: this.shutterType_,
            docResult,
            docFixType: fixType,
          });
        };

        const doRecrop = async () => {
          const {corners: newCorners, rotation} =
              await this.cropDocument_.reviewCropArea(corners);

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
          await this.review_.setReviewPhoto(docBlob);
        };

        await this.cropDocument_.setReviewPhoto(originImage.blob);
        if (needFirstRecrop) {
          nav.close(ViewName.FLASH);
          await doRecrop();
        } else {
          docBlob = await doCrop(originImage.blob, corners, Rotation.ANGLE_0);
          await this.review_.setReviewPhoto(docBlob);
          nav.close(ViewName.FLASH);
        }

        const positive = new review.Options(
            new review.Option(I18nString.LABEL_SAVE_PDF_DOCUMENT, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PDF);
              },
              exitValue: MimeType.PDF,
            }),
            new review.Option(I18nString.LABEL_SAVE_PHOTO_DOCUMENT, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PHOTO);
              },
              exitValue: MimeType.JPEG,
            }),
            new review.Option(I18nString.LABEL_SHARE, {
              callback: async () => {
                sendEvent(metrics.DocResultType.SHARE);
                const type = MimeType.JPEG;
                const name = (new Filenamer()).newDocumentName(type);
                await util.share(new File([docBlob], name, {type}));
              },
            }),
        );

        const optionsArgs = [
          new review.Option(I18nString.LABEL_RETAKE, {
            callback: () => {
              sendEvent(metrics.DocResultType.CANCELED);
            },
            exitValue: null,
          }),
        ];
        if (allowRecrop) {
          optionsArgs.unshift(new review.Option(I18nString.LABEL_FIX_DOCUMENT, {
            callback: doRecrop,
            hasPopup: true,
          }));
        }
        const negative = new review.Options(...optionsArgs);

        const mimeType = await this.review_.startReview({positive, negative});
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

  /**
   * @override
   */
  createVideoSaver() {
    return this.resultSaver_.startSaveVideo(this.outputVideoRotation_);
  }

  /**
   * @override
   */
  getPreviewVideo() {
    const video = this.preview_.getVideoElement();
    assertInstanceof(video, HTMLVideoElement);
    return video;
  }

  /**
   * @override
   */
  playShutterEffect() {
    sound.play(dom.get('#sound-shutter', HTMLAudioElement));
    animate.play(this.preview_.getVideoElement());
  }

  /**
   * @override
   */
  waitPreviewReady() {
    return this.preview_.waitReadyForTakePhoto();
  }

  /**
   * @override
   */
  async handleResultVideo({resolution, duration, videoSaver, everPaused}) {
    metrics.sendCaptureEvent({
      recordType: metrics.RecordType.NORMAL_VIDEO,
      facing: this.facingMode_,
      duration,
      resolution,
      shutterType: this.shutterType_,
      everPaused,
    });
    try {
      await this.resultSaver_.finishSaveVideo(videoSaver);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * @override
   */
  async handleResultGif({name, getBlob, resolution, duration}) {
    nav.open(ViewName.FLASH);
    const blob = await getBlob();
    const sendEvent = (gifResult) => {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.GIF,
        facing: this.facingMode_,
        resolution,
        duration,
        shutterType: this.shutterType_,
        gifResult,
      });
    };

    let result = false;
    await this.prepareReview_(async () => {
      await this.review_.setReviewPhoto(blob);
      const positive = new review.Options(
          new review.Option(I18nString.LABEL_SAVE, {exitValue: true}),
          new review.Option(I18nString.LABEL_SHARE, {
            callback: async () => {
              sendEvent(metrics.GifResultType.SHARE);
              await util.share(new File([blob], name, {type: MimeType.GIF}));
            },
          }),
      );
      const negative = new review.Options(
          new review.Option(I18nString.LABEL_RETAKE, {exitValue: null}));
      nav.close(ViewName.FLASH);
      result = await this.review_.startReview({positive, negative});
    });
    if (result) {
      sendEvent(metrics.GifResultType.SAVE);
      await this.resultSaver_.saveGif(blob, name);
    } else {
      sendEvent(metrics.GifResultType.RETAKE);
    }
  }

  /**
   * @override
   */
  layout() {
    this.layout_.update();
  }

  /**
   * @override
   */
  handlingKey(key) {
    if (key === 'Ctrl-R') {
      toast.showDebugMessage(this.preview_.toString());
      return true;
    }
    if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
        state.get(state.State.TABLET) && state.get(state.State.STREAMING)) {
      if (state.get(state.State.TAKING)) {
        this.endTake_();
      } else {
        this.beginTake(metrics.ShutterType.VOLUME_KEY);
      }
      return true;
    }
    return false;
  }

  /**
   * Stops camera and tries to start camera stream again if possible.
   * @return {!Promise<boolean>} Promise resolved to whether start camera
   *     successfully.
   */
  async start() {
    // To prevent multiple callers enter this function at the same time, wait
    // until previous caller resets configuring to null.
    while (this.configuring_ !== null) {
      if (!await this.configuring_) {
        // Retry will be kicked out soon.
        return false;
      }
    }
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.configuring_ = (async () => {
      try {
        if (state.get(state.State.TAKING)) {
          await this.endTake_();
        }
      } finally {
        await this.stopStreams_();
      }
      return this.start_();
    })();
    return this.configuring_;
  }

  /**
   * Try start stream reconfiguration with specified mode and device id.
   * @param {string} deviceId
   * @param {!Mode} mode
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithMode_(deviceId, mode) {
    const deviceOperator = await DeviceOperator.getInstance();
    state.set(state.State.USE_FAKE_CAMERA, deviceOperator === null);
    let resolCandidates;
    let photoRs;
    if (deviceOperator) {
      resolCandidates = this.modes_.getResolutionCandidates(mode, deviceId);
      photoRs = await deviceOperator.getPhotoResolutions(deviceId);
    } else {
      resolCandidates = this.modes_.getFakeResolutionCandidates(mode, deviceId);
      photoRs = resolCandidates.map((c) => c.resolution);
    }
    const maxResolution =
        photoRs.reduce((maxR, r) => r.area > maxR.area ? r : maxR);
    for (const {resolution: captureR, previewCandidates} of resolCandidates) {
      for (const constraints of previewCandidates) {
        if (this.isSuspended()) {
          throw new CameraSuspendedError();
        }
        const videoSnapshotResolution =
            state.get(state.State.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT) ?
            maxResolution :
            captureR;
        this.modes_.setCaptureParams(
            mode, constraints, captureR, videoSnapshotResolution);
        try {
          await this.modes_.prepareDevice();
          const factory = this.modes_.getModeFactory(mode);

          // Sets 2500 ms delay between screen resumed and open camera preview.
          // TODO(b/173679752): Removes this workaround after fix delay on
          // kernel side.
          if (loadTimeData.getBoard() === 'zork') {
            const screenOnTime = performance.now() - this.lastScreenOnTime_;
            const delay = 2500 - screenOnTime;
            if (delay > 0) {
              await util.sleep(delay);
            }
          }
          const stream = await this.preview_.open(constraints);
          this.facingMode_ = this.preview_.getFacing();

          let enablePTZ = this.preview_.isSupportPTZ();
          if (enablePTZ) {
            const modeSupport = deviceOperator === null ||
                this.modes_.isSupportPTZ(
                    mode,
                    captureR,
                    this.preview_.getResolution(),
                );
            if (!modeSupport) {
              await this.preview_.resetPTZ();
              enablePTZ = false;
            }
          }
          state.set(state.State.ENABLE_PTZ, enablePTZ);

          this.options_.updateValues(stream, this.facingMode_);
          factory.setPreviewStream(stream);
          factory.setFacing(this.facingMode_);

          await this.modes_.updateModeSelectionUI(deviceId);
          await this.modes_.updateMode(
              factory, stream, this.facingMode_, deviceId);
          await this.scanOptions_.attachPreview(
              this.preview_.getVideoElement());
          for (const l of this.configureCompleteListener_) {
            l();
          }
          nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
          return true;
        } catch (e) {
          await this.stopStreams_();

          let errorToReport = e;
          // Since OverconstrainedError is not an Error instance.
          if (e instanceof OverconstrainedError) {
            errorToReport =
                new Error(`${e.message} (constraint = ${e.constraint})`);
            errorToReport.name = 'OverconstrainedError';
          } else if (e.name === 'NotReadableError') {
            // TODO(b/187879603): Remove this hacked once we understand more
            // about such error.
            // We cannot get the camera facing from stream since it might not be
            // successfully opened. Therefore, we asked the camera facing via
            // Mojo API.
            let facing = Facing.NOT_SET;
            if (deviceOperator !== null) {
              facing = await deviceOperator.getCameraFacing(deviceId);
            }
            errorToReport = new Error(`${e.message} (facing = ${facing})`);
            errorToReport.name = 'NotReadableError';
          }
          error.reportError(
              ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
              assertInstanceof(errorToReport, Error));
        }
      }
    }
    return false;
  }

  /**
   * Try start stream reconfiguration with specified device id.
   * @param {string} deviceId
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithDevice_(deviceId) {
    const supportedModes = await this.modes_.getSupportedModes(deviceId);
    const modes = this.modes_.getModeCandidates().filter(
        (m) => supportedModes.includes(m));
    for (const mode of modes) {
      if (await this.startWithMode_(deviceId, mode)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Starts camera configuration process.
   * @return {!Promise<boolean>} Resolved to boolean for whether the
   *     configuration is succeeded or kicks out another round of
   *     reconfiguration.
   * @private
   */
  async start_() {
    try {
      await this.infoUpdater_.lockDeviceInfo(async () => {
        if (!this.isSuspended()) {
          for (const id of this.options_.videoDeviceIds(this.facingMode_)) {
            if (await this.startWithDevice_(id)) {
              // Make the different active camera announced by screen reader.
              const currentId = this.options_.currentDeviceId;
              assert(currentId !== null);
              if (currentId === this.activeDeviceId_) {
                return;
              }
              this.activeDeviceId_ = currentId;
              const info = this.infoUpdater_.getDeviceInfo(currentId);
              if (info !== null) {
                toast.speak(I18nString.STATUS_MSG_CAMERA_SWITCHED, info.label);
              }
              return;
            }
          }
        }
        throw new CameraSuspendedError();
      });
      this.configuring_ = null;
      state.set(state.State.CAMERA_CONFIGURING, false);

      return true;
    } catch (e) {
      this.activeDeviceId_ = null;
      if (!(e instanceof CameraSuspendedError)) {
        error.reportError(
            ErrorType.START_CAMERA_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
        nav.open(ViewName.WARNING, WarningType.NO_CAMERA);
      }
      // Schedule to retry.
      if (this.retryStartTimeout_) {
        clearTimeout(this.retryStartTimeout_);
        this.retryStartTimeout_ = null;
      }
      this.retryStartTimeout_ = setTimeout(() => {
        this.configuring_ = this.start_();
      }, 100);

      this.perfLogger_.interrupt();
      return false;
    }
  }

  /**
   * Stop extra stream and preview stream.
   * @private
   */
  async stopStreams_() {
    await this.modes_.clear();
    await this.preview_.close();
    await this.scanOptions_.detachPreview();
  }
}
