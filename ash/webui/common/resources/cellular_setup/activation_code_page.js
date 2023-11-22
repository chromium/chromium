// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Page in eSIM Setup flow that accepts activation code.
 * User has option for manual entry or scan a QR code.
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import './base_page.js';
import './cellular_setup_icons.html.js';

import {focusWithoutInk} from '//resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {MojoInterfaceProviderImpl} from '//resources/ash/common/network/mojo_interface_provider.js';
import {afterNextRender, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrosNetworkConfig, CrosNetworkConfigInterface} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {getTemplate} from './activation_code_page.html.js';

/**
 * @type {!number}
 * @private
 */
const QR_CODE_DETECTION_INTERVAL_MS = 1000;

/** @enum {number} */
const PageState = {
  MANUAL_ENTRY: 1,
  SCANNING_USER_FACING: 2,
  SCANNING_ENVIRONMENT_FACING: 3,
  SWITCHING_CAM_USER_TO_ENVIRONMENT: 4,
  SWITCHING_CAM_ENVIRONMENT_TO_USER: 5,
  SCANNING_SUCCESS: 6,
  SCANNING_FAILURE: 7,
  MANUAL_ENTRY_INSTALL_FAILURE: 8,
  SCANNING_INSTALL_FAILURE: 9,
};

/** @enum {number} */
const UiElement = {
  START_SCANNING: 1,
  VIDEO: 2,
  SWITCH_CAMERA: 3,
  SCAN_FINISH: 4,
  SCAN_SUCCESS: 5,
  SCAN_FAILURE: 6,
  CODE_DETECTED: 7,
  SCAN_INSTALL_FAILURE: 8,
};

/**
 * barcode format used by |BarcodeDetector|
 * @private {string}
 */
const QR_CODE_FORMAT = 'qr_code';

/**
 * The prefix for valid activation codes.
 * @private {string}
 */
const ACTIVATION_CODE_PREFIX = 'LPA:1$';

Polymer({
  _template: getTemplate(),
  is: 'activation-code-page',

  behaviors: [I18nBehavior],

  properties: {
    activationCode: {
      type: String,
      notify: true,
      observer: 'onActivationCodeChanged_',
    },

    showError: {
      type: Boolean,
      notify: true,
      observer: 'onShowErrorChanged_',
    },

    /**
     * Readonly property indicating whether the current |activationCode|
     * was scanned from QR code.
     */
    isFromQrCode: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * Indicates the UI is busy with an operation and cannot be interacted with.
     */
    showBusy: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates no profiles were found while scanning.
     */
    showNoProfilesFound: {
      type: Boolean,
      notify: true,
    },

    /**
     * Enum used as an ID for specific UI elements.
     * A UiElement is passed between html and JS for
     * certain UI elements to determine their state.
     *
     * @type {!UiElement}
     */
    UiElement: {
      type: Object,
      value: UiElement,
    },

    /**
     * @type {!PageState}
     * @private
     */
    state_: {
      type: Object,
      value: PageState,
      observer: 'onStateChanged_',
    },

    /** @private */
    cameraCount_: {
      type: Number,
      value: 0,
      observer: 'onHasCameraCountChanged_',
    },

    /**
     *  TODO(crbug.com/1093185): add type |BarcodeDetector| when externs
     *  becomes available
     *  @private {?Object}
     */
    qrCodeDetector_: {
      type: Object,
      value: null,
    },

    /**
     * If true, video is expanded.
     */
    expanded_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * A11y string used to announce the current status of qr code camera
     * detection. Used when device web cam is turned on and ready to scan,
     * and also used after scan has been completed.
     * @private
     */
    qrCodeCameraA11yString_: {
      type: String,
      value: '',
    },

    /**
     * If true, device is locked to specific cellular operator.
     */
    isDeviceCarrierLocked_: {
      type: Boolean,
      value: false,
    },

    isCellularCarrierLockEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('isCellularCarrierLockEnabled') &&
            loadTimeData.getBoolean('isCellularCarrierLockEnabled');
      },
    },

    /**
     * Indicates whether or not |activationCode| matches the correct activation
     * code format. If there is a partial match (i.e. the code is incomplete but
     * matches the format so far), this will be false.
     * @private
     */
    isActivationCodeInvalidFormat_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?CrosNetworkConfigInterface} */
  networkConfig_: null,

  /** @override */
  created() {
    if (!this.isCellularCarrierLockEnabled_) {
      return;
    }
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.networkConfig_.getDeviceStateList().then(response => {
      const devices = response.result;
      const deviceState =
          devices.find(device => device.type == NetworkType.kCellular) || null;
      if (deviceState) {
        this.isDeviceCarrierLocked_ = deviceState.isCarrierLocked;
      }
    });
  },

  /**
   * @type {MediaDevices}
   * @private
   */
  mediaDevices_: null,

  /**
   * @type {?MediaStream}
   * @private
   */
  stream_: null,

  /**
   * @type {?number}
   * @private
   */
  qrCodeDetectorTimer_: null,


  /**
   * The function used to initiate a repeating timer. Can be overwritten in
   * tests.
   * @private {function(Function, number)}
   */
  setIntervalFunction_: setInterval.bind(window),

  /**
   *  TODO(crbug.com/1093185): add type |BarcodeDetector| when externs
   *  becomes available
   *  @suppress {undefinedVars|missingProperties}
   *  @private
   */
  barcodeDetectorClass_: BarcodeDetector,

  /** @private {typeof ImageCapture} */
  imageCaptureClass_: ImageCapture,

  /**
   * Function used to play the video. Can be overwritten by
   * setFakesForTesting().
   * @private {function()}
   */
  playVideo_: function() {
    this.$$('#video').play();
  },

  /**
   * Function used to stop a stream. Can be overwritten by setFakesForTesting().
   * @private {function(MediaStream)}
   */
  stopStream_: function(stream) {
    if (stream) {
      stream.getTracks()[0].stop();
    }
  },

  /** @override */
  ready() {
    this.setMediaDevices(navigator.mediaDevices);
    this.initBarcodeDetector_();
    this.state_ = PageState.MANUAL_ENTRY;
  },

  /** @override */
  detached() {
    this.stopStream_(this.stream_);
    if (this.qrCodeDetectorTimer_) {
      this.clearQrCodeDetectorTimer_();
    }
    this.mediaDevices_.removeEventListener(
        'devicechange', this.updateCameraCount_.bind(this));
  },

  /**
   * @return {boolean}
   * @private
   */
  isScanningAvailable_() {
    return this.cameraCount_ > 0 && !!this.qrCodeDetector_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCarrierLockWarning_() {
    return this.isCellularCarrierLockEnabled_ && this.isDeviceCarrierLocked_;
  },

  /**
   * TODO(crbug.com/1093185): Remove suppression when shape_detection extern
   * definitions become available.
   * @suppress {undefinedVars|missingProperties}
   * @private
   */
  async initBarcodeDetector_() {
    const formats = await this.barcodeDetectorClass_.getSupportedFormats();

    if (!formats || formats.length === 0) {
      this.qrCodeDetector_ = null;
      return;
    }

    const qrCodeFormat = formats.find(format => format === QR_CODE_FORMAT);
    if (qrCodeFormat) {
      this.qrCodeDetector_ =
          new this.barcodeDetectorClass_({formats: [QR_CODE_FORMAT]});
    }
  },

  /**
   * @param {MediaDevices} mediaDevices
   */
  setMediaDevices(mediaDevices) {
    this.mediaDevices_ = mediaDevices;
    this.updateCameraCount_();
    this.mediaDevices_.addEventListener(
        'devicechange', this.updateCameraCount_.bind(this));
  },

  /**
   * TODO(crbug.com/1093185): Add barcodeDetectorClass type when BarcodeDetector
   * externs become available.
   * @param barcodeDetectorClass
   * @param {typeof ImageCapture} imageCaptureClass
   * @param {function(Function, number)} setIntervalFunction
   * @param {function()} playVideoFunction
   * @param {function(MediaStream)} stopStreamFunction
   */
  async setFakesForTesting(
      barcodeDetectorClass, imageCaptureClass, setIntervalFunction,
      playVideoFunction, stopStreamFunction) {
    this.barcodeDetectorClass_ = barcodeDetectorClass;
    await this.initBarcodeDetector_();
    this.imageCaptureClass_ = imageCaptureClass;
    this.setIntervalFunction_ = setIntervalFunction;
    this.playVideo_ = playVideoFunction;
    this.stopStream_ = stopStreamFunction;
  },

  /**
   * @returns {?number}
   */
  getQrCodeDetectorTimerForTest() {
    return this.qrCodeDetectorTimer_;
  },

  /**
   * @return {string}
   * @private
   */
  computeActivationCodeClass_() {
    return this.isScanningAvailable_() ? 'relative' : 'center width-92';
  },

  /** @private */
  updateCameraCount_() {
    if (!this.mediaDevices_ || !this.mediaDevices_.enumerateDevices) {
      this.cameraCount_ = 0;
      return;
    }

    this.mediaDevices_.enumerateDevices()
        .then(devices => {
          this.cameraCount_ =
              devices.filter(device => device.kind === 'videoinput').length;
        })
        .catch(e => {
          this.cameraCount_ = 0;
        });
  },

  /** @private */
  onHasCameraCountChanged_() {
    // If the user was using an environment-facing camera and it was removed,
    // restart scanning with the user-facing camera.
    if ((this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) &&
        this.cameraCount_ === 1) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      this.startScanning_();
    }
  },

  /** private */
  startScanning_() {
    const oldStream = this.stream_;
    if (this.qrCodeDetectorTimer_) {
      this.clearQrCodeDetectorTimer_();
    }

    const useUserFacingCamera =
        this.state_ !== PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    this.mediaDevices_
        .getUserMedia({
          video: {
            height: 130,
            width: 482,
            facingMode: useUserFacingCamera ? 'user' : 'environment',
          },
          audio: false,
        })
        .then(stream => {
          this.stream_ = stream;
          if (this.stream_) {
            const video = this.$$('#video');
            video.srcObject = stream;
            this.playVideo_();
          }
          this.stopStream_(oldStream);

          this.activationCode = '';
          this.state_ = useUserFacingCamera ?
              PageState.SCANNING_USER_FACING :
              PageState.SCANNING_ENVIRONMENT_FACING;

          if (this.stream_) {
            this.detectQrCode_();
          }
        })
        .catch(e => {
          this.state_ = PageState.SCANNING_FAILURE;
        });
  },

  /**
   * Continuously checks stream if it contains a QR code. If a QR code is
   * detected, activationCode is set to the QR code's value and the detection
   * stops.
   * @private
   */
  async detectQrCode_() {
    try {
      this.qrCodeDetectorTimer_ = this.setIntervalFunction_(
          (async () => {
            const capturer =
                new this.imageCaptureClass_(this.stream_.getVideoTracks()[0]);
            const frame = await capturer.grabFrame();
            const activationCode = await this.detectActivationCode_(frame);
            if (activationCode) {
              this.clearQrCodeDetectorTimer_();
              this.activationCode = activationCode;
              this.stopStream_(this.stream_);

              if (this.validateActivationCode_(activationCode)) {
                this.state_ = PageState.SCANNING_SUCCESS;
              } else {
                // If the scanned activation code is invalid or incomplete, show
                // error.
                this.state_ = PageState.SCANNING_INSTALL_FAILURE;
              }
            }
          }),
          QR_CODE_DETECTION_INTERVAL_MS);
    } catch (error) {
      this.state_ = PageState.SCANNING_FAILURE;
    }
  },

  /**
   * @param {ImageBitmap} frame
   * @return {!Promise<string|null>}
   * TODO(crbug.com/1093185): Remove suppression when shape_detection extern
   * definitions become available.
   * @suppress {undefinedVars|missingProperties}
   * @private
   */
  async detectActivationCode_(frame) {
    if (!this.qrCodeDetector_) {
      return null;
    }

    const qrCodes = await this.qrCodeDetector_.detect(frame);
    if (qrCodes.length > 0) {
      return qrCodes[0].rawValue;
    }
    return null;
  },

  /** @private */
  onActivationCodeChanged_() {
    this.fire('activation-code-updated', {
      activationCode: this.validateActivationCode_(this.activationCode) ?
          this.activationCode :
          null,
    });
  },

  /** @private */
  clearQrCodeDetectorTimer_() {
    clearTimeout(this.qrCodeDetectorTimer_);
    this.qrCodeDetectorTimer_ = null;
  },

  /**
   * Checks if |activationCode| matches or partially matches the correct format.
   * Sets |isActivationCodeInvalidFormat_| to true if the format is incorrect.
   * @param {string} activationCode
   * @return {boolean} Returns true if |activationCode| is valid and ready to be
   *     submitted for installation.
   * @private
   */
  validateActivationCode_(activationCode) {
    if (activationCode.length <= ACTIVATION_CODE_PREFIX.length) {
      // If the currently entered activation code is shorter than
      // |ACTIVATION_CODE_PREFIX|, check if the code matches the format thus
      // far.
      this.isActivationCodeInvalidFormat_ = activationCode !==
          ACTIVATION_CODE_PREFIX.substring(0, activationCode.length);

      // Because the entered activation code is shorter than
      // |ACTIVATION_CODE_PREFIX| it cannot be submitted yet.
      return false;
    } else {
      // |activationCode| is longer than |ACTIVATION_CODE_PREFIX|. Check if it
      // begins with the prefix.
      this.isActivationCodeInvalidFormat_ =
          activationCode.substring(0, ACTIVATION_CODE_PREFIX.length) !==
          ACTIVATION_CODE_PREFIX;
    }

    if (this.isActivationCodeInvalidFormat_) {
      // If the activation code does not match the format, it cannot be
      // submitted.
      return false;
    }
    return true;
  },

  /** @private */
  onSwitchCameraButtonPressed_() {
    if (this.state_ === PageState.SCANNING_USER_FACING) {
      this.state_ = PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    } else if (this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
    }
    this.startScanning_();
  },

  /** @private */
  onShowErrorChanged_() {
    if (this.showError) {
      if (this.state_ === PageState.MANUAL_ENTRY) {
        this.state_ = PageState.MANUAL_ENTRY_INSTALL_FAILURE;
        afterNextRender(this, () => {
          focusWithoutInk(this.$.activationCode);
        });
      } else if (this.state_ === PageState.SCANNING_SUCCESS) {
        this.state_ = PageState.SCANNING_INSTALL_FAILURE;
      }
    }
  },

  /** @private */
  onStateChanged_() {
    this.qrCodeCameraA11yString_ = '';
    if (this.state_ !== PageState.MANUAL_ENTRY_INSTALL_FAILURE &&
        this.state_ !== PageState.SCANNING_INSTALL_FAILURE) {
      this.showError = false;
    }
    if (this.state_ === PageState.MANUAL_ENTRY) {
      this.isFromQrCode = false;

      // Clear |qrCodeDetectorTimer_| before closing video stream, prevents
      // image capturer from going into an inactive state and throwing errors
      // when |grabFrame()| is called.
      this.clearQrCodeDetectorTimer_();

      // Wait for the video element to be hidden by isUiElementHidden() before
      // stopping the stream or the user will see a flash.
      afterNextRender(this, () => {
        this.stopStream_(this.stream_);
      });
    }

    if (this.state_ === PageState.SCANNING_USER_FACING ||
        this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) {
      this.qrCodeCameraA11yString_ = this.i18n('qrCodeA11YCameraOn');
      this.expanded_ = true;
      return;
    }

    // Focus on the next button after scanning is successful.
    if (this.state_ === PageState.SCANNING_SUCCESS) {
      this.isFromQrCode = true;
      this.qrCodeCameraA11yString_ = this.i18n('qrCodeA11YCameraScanSuccess');
      this.fire('focus-default-button');
    }

    this.expanded_ = false;
  },

  /**
   * @param {KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key === 'Enter') {
      this.fire('forward-navigation-requested');
    }

    // Prevents barcode detector video from closing if user tabs through
    // window. We should only close barcode detector window if user
    // types in activation code input.
    if (e.key === 'Tab') {
      return;
    }

    this.state_ = PageState.MANUAL_ENTRY;
    e.stopPropagation();
  },

  /**
   * @param {UiElement} uiElement
   * @param {PageState} state
   * @param {number} cameraCount
   * @private
   */
  isUiElementHidden_(uiElement, state, cameraCount) {
    switch (uiElement) {
      case UiElement.START_SCANNING:
        return state !== PageState.MANUAL_ENTRY &&
            state !== PageState.MANUAL_ENTRY_INSTALL_FAILURE;
      case UiElement.VIDEO:
        return state !== PageState.SCANNING_USER_FACING &&
            state !== PageState.SCANNING_ENVIRONMENT_FACING;
      case UiElement.SWITCH_CAMERA:
        const isScanning = state === PageState.SCANNING_USER_FACING ||
            state === PageState.SCANNING_ENVIRONMENT_FACING;
        return !(isScanning && this.cameraCount_ > 1);
      case UiElement.SCAN_FINISH:
        return state !== PageState.SCANNING_SUCCESS &&
            state !== PageState.SCANNING_FAILURE &&
            state !== PageState.SCANNING_INSTALL_FAILURE;
      case UiElement.SCAN_SUCCESS:
        return state !== PageState.SCANNING_SUCCESS &&
            state !== PageState.SCANNING_INSTALL_FAILURE;
      case UiElement.SCAN_FAILURE:
        return state !== PageState.SCANNING_FAILURE;
      case UiElement.CODE_DETECTED:
        return state !== PageState.SCANNING_SUCCESS;
      case UiElement.SCAN_INSTALL_FAILURE:
        return state !== PageState.SCANNING_INSTALL_FAILURE;
    }
  },

  /**
   * @param {UiElement} uiElement
   * @param {PageState} state
   * @param {boolean} showBusy
   * @private
   */
  isUiElementDisabled_(uiElement, state, showBusy) {
    if (showBusy) {
      return true;
    }
    switch (uiElement) {
      case UiElement.SWITCH_CAMERA:
        return state === PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT ||
            state === PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      default:
        return false;
    }
  },

  /**
   * @return {string}
   * @private
   */
  getDescription_() {
    if (!this.isScanningAvailable_()) {
      if (this.showNoProfilesFound) {
        return this.i18n('enterActivationCodeNoProfilesFound');
      }
      return this.i18n('enterActivationCode');
    }
    if (this.showNoProfilesFound) {
      return this.i18n('scanQRCodeNoProfilesFound');
    }
    return this.i18n('scanQRCode');
  },

  /**
   * @param {PageState} state
   * @return {boolean}
   * @private
   */
  shouldActivationCodeInputBeInvalid_(state) {
    if (this.isActivationCodeInvalidFormat_) {
      return true;
    }
    return state === PageState.MANUAL_ENTRY_INSTALL_FAILURE;
  },

  /**
   * @param {boolean} showBusy
   * @return {string}
   * @private
   */
  getInputSubtitle_(showBusy) {
    if (showBusy) {
      return this.i18n('scanQrCodeLoading');
    }

    // Because this string contains '<' and '>' characters, we cannot use i18n
    // methods.
    return loadTimeData.getString('scanQrCodeInputSubtitle');
  },

  /**
   * @return {string}
   * @private
   */
  getInputErrorMessage_() {
    // Because this string contains '<' and '>' characters, we cannot use i18n
    // methods.
    return loadTimeData.getString('scanQrCodeInputError');
  },
});
