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
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import './base_page.js';
import './cellular_setup_icons.html.js';

import type {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {MojoInterfaceProviderImpl} from '//resources/ash/common/network/mojo_interface_provider.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrosNetworkConfigInterface} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activation_code_page.html.js';

const QR_CODE_DETECTION_INTERVAL_MS = 1000;

enum PageState {
  MANUAL_ENTRY = 1,
  SCANNING_USER_FACING = 2,
  SCANNING_ENVIRONMENT_FACING = 3,
  SWITCHING_CAM_USER_TO_ENVIRONMENT = 4,
  SWITCHING_CAM_ENVIRONMENT_TO_USER = 5,
  SCANNING_SUCCESS = 6,
  SCANNING_FAILURE = 7,
  MANUAL_ENTRY_INSTALL_FAILURE = 8,
  SCANNING_INSTALL_FAILURE = 9,
}

enum UiElement {
  START_SCANNING = 1,
  VIDEO = 2,
  SWITCH_CAMERA = 3,
  SCAN_FINISH = 4,
  SCAN_SUCCESS = 5,
  SCAN_FAILURE = 6,
  CODE_DETECTED = 7,
  SCAN_INSTALL_FAILURE = 8,
}

/**
 * barcode format used by |BarcodeDetector|
 */
const QR_CODE_FORMAT = 'qr_code';

/**
 * The prefix for valid activation codes.
 */
const ACTIVATION_CODE_PREFIX = 'LPA:1$';

export interface ActivationCodePageElement {
  $: {
    activationCode: CrInputElement,
  };
}

const ActivationCodePageElementBase = I18nMixin(PolymerElement);

export class ActivationCodePageElement extends ActivationCodePageElementBase {
  static get is() {
    return 'activation-code-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       */
      UiElement: {
        type: Object,
        value: UiElement,
      },

      state_: {
        type: Object,
        value: PageState,
        observer: 'onStateChanged_',
      },

      cameraCount_: {
        type: Number,
        value: 0,
        observer: 'onHasCameraCountChanged_',
      },

      /**
       *  TODO(crbug.com/40134918): add type |BarcodeDetector| when externs
       *  becomes available
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

      /**
       * Indicates whether or not |activationCode| matches the correct
       * activation code format. If there is a partial match (i.e. the code is
       * incomplete but matches the format so far), this will be false.
       */
      isActivationCodeInvalidFormat_: {
        type: Boolean,
        value: false,
      },
    };
  }

  activationCode: string;
  showError: boolean;
  isFromQrCode: boolean;
  showNoProfilesFound: boolean;
  private state_: PageState;
  private cameraCount_: number;
  private qrCodeDetector_: BarcodeDetector|null = null;
  private expanded_: boolean;
  private qrCodeCameraA11yString_: string;
  private isDeviceCarrierLocked_: boolean;
  private isActivationCodeInvalidFormat_: boolean;
  private networkConfig_: CrosNetworkConfigInterface|null = null;
  private mediaDevices_: MediaDevices|null = null;
  private stream_: MediaStream|null = null;
  private qrCodeDetectorTimer_: number|null = null;

  /**
   * The function used to initiate a repeating timer. Can be overwritten in
   * tests.
   */
  private setIntervalFunction_: (callback: Function, interval: number)
      => number = setInterval.bind(window);
  private barcodeDetectorClass_ = BarcodeDetector;
  private imageCaptureClass_ = ImageCapture;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.networkConfig_!.getDeviceStateList().then(response => {
      const devices = response.result;
      const deviceState =
          devices.find(device => device.type == NetworkType.kCellular) || null;
      if (deviceState) {
        this.isDeviceCarrierLocked_ = deviceState.isCarrierLocked;
      }
    });
  }

  override ready() {
    super.ready();

    this.setMediaDevices(navigator.mediaDevices);
    this.initBarcodeDetector_();
    this.state_ = PageState.MANUAL_ENTRY;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.stopStream_(this.stream_);
    if (this.qrCodeDetectorTimer_) {
      this.clearQrCodeDetectorTimer_();
    }
    this.mediaDevices_!.removeEventListener(
        'devicechange', this.updateCameraCount_.bind(this));
  }

  /**
   * Function used to play the video. Can be overwritten by
   * setFakesForTesting().
   */
  private playVideo_(): void {
    const videoElement = this.shadowRoot!.querySelector<HTMLVideoElement>('#video');
    if (videoElement) {
      assert(this.stream_);
      videoElement.srcObject = this.stream_;
      videoElement.play();
    }
  }

  /**
   * Function used to stop a stream.
   */
  private stopStream_(stream: MediaStream|null): void {
    if (stream) {
      for (const track of stream.getTracks()) {
        track.stop();
      }
    }
  }

  private isScanningAvailable_(): boolean {
    return this.cameraCount_ > 0 && !!this.qrCodeDetector_;
  }

  private shouldShowCarrierLockWarning_(): boolean {
    return this.isDeviceCarrierLocked_;
  }

  /**
   * TODO(crbug.com/40134918): Remove suppression when shape_detection extern
   * definitions become available.
   */
  private async initBarcodeDetector_(): Promise<void> {
    const formats = await this.barcodeDetectorClass_.getSupportedFormats();

    if (!formats || formats.length === 0) {
      this.qrCodeDetector_ = null;
      return;
    }

    const qrCodeFormat = formats.find(
        (format: BarcodeFormat) => format === QR_CODE_FORMAT);
    if (qrCodeFormat) {
      this.qrCodeDetector_ =
          new this.barcodeDetectorClass_({formats: [QR_CODE_FORMAT]});
    }
  }

  setMediaDevices(mediaDevices: MediaDevices): void {
    this.mediaDevices_ = mediaDevices;
    this.updateCameraCount_();
    this.mediaDevices_.addEventListener(
        'devicechange', this.updateCameraCount_.bind(this));
  }

  async setFakesForTesting(
      barcodeDetectorClass: typeof BarcodeDetector,
      imageCaptureClass: typeof ImageCapture,
      setIntervalFunction: (callback: Function, interval: number) => number,
      playVideoFunction: () => void): Promise<void> {
    this.barcodeDetectorClass_ = barcodeDetectorClass;
    await this.initBarcodeDetector_();
    this.imageCaptureClass_ = imageCaptureClass;
    this.setIntervalFunction_ = setIntervalFunction;
    this.playVideo_ = playVideoFunction;
  }

  getQrCodeDetectorTimerForTest(): number|null {
    return this.qrCodeDetectorTimer_;
  }

  attemptToFocusOnPageContent(): boolean {
    // Prioritize focusing the camera button if scanning is available.
    // TODO(b/332925540): Add interactive test for button focus.
    if (this.isScanningAvailable_()) {
      const useCameraBtn = this.shadowRoot!.querySelector<CrButtonElement>(
          '#startScanningButton');

      if (useCameraBtn) {
        useCameraBtn.focus();
        return true;
      }
    }

    // Fallback: Focus on the activation code input
    const activationCodeInput =
        this.shadowRoot!.querySelector<CrInputElement>('#activationCode');
    if (activationCodeInput) {
      activationCodeInput.focus();
      return true;
    }

    return false;
  }

  private computeActivationCodeClass_(): string {
    return this.isScanningAvailable_() ? 'relative' : 'center';
  }

  private updateCameraCount_(): void {
    if (!this.mediaDevices_ || !this.mediaDevices_.enumerateDevices) {
      this.cameraCount_ = 0;
      return;
    }

    this.mediaDevices_.enumerateDevices()
        .then(devices => {
          this.cameraCount_ =
              devices.filter(device => device.kind === 'videoinput').length;
        })
        .catch(() => {
          this.cameraCount_ = 0;
        });
  }

  private onHasCameraCountChanged_(): void {
    // If the user was using an environment-facing camera and it was removed,
    // restart scanning with the user-facing camera.
    if ((this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) &&
        this.cameraCount_ === 1) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      this.startScanning_();
    }
  }

  private startScanning_(): void {
    if (this.qrCodeDetectorTimer_) {
      this.clearQrCodeDetectorTimer_();
    }

    if (this.stream_) {
      this.stopStream_(this.stream_);
    }

    const useUserFacingCamera =
        this.state_ !== PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    this.mediaDevices_!
        .getUserMedia({
          video: {
            height: 130,
            width: 482,
            facingMode: useUserFacingCamera ? 'user' : 'environment',
          },
          audio: false,
        })
        .then((stream: MediaStream) => {
          this.stream_ = stream;
          if (this.stream_) {
            this.playVideo_();
          }

          this.activationCode = '';
          this.state_ = useUserFacingCamera ?
              PageState.SCANNING_USER_FACING :
              PageState.SCANNING_ENVIRONMENT_FACING;

          if (this.stream_) {
            this.detectQrCode_();
          }
        })
        .catch(() => {
          this.state_ = PageState.SCANNING_FAILURE;
        });
  }

  /**
   * Continuously checks stream if it contains a QR code. If a QR code is
   * detected, activationCode is set to the QR code's value and the detection
   * stops.
   */
  private async detectQrCode_(): Promise<void> {
    try {
      this.qrCodeDetectorTimer_ = this.setIntervalFunction_(
          (async () => {
            assert(!!this.stream_);
            const capturer =
                new this.imageCaptureClass_(this.stream_.getVideoTracks()[0]);
            const frame = await capturer.grabFrame();
            const activationCode = await this.detectActivationCode_(frame);
            if (activationCode) {
              if (this.qrCodeDetectorTimer_) {
                this.clearQrCodeDetectorTimer_();
              }
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
  }

  /**
   * TODO(crbug.com/40134918): Remove suppression when shape_detection extern
   * definitions become available.
   */
  private async detectActivationCode_(frame: ImageBitmap):
      Promise<string|null> {
    if (!this.qrCodeDetector_) {
      return null;
    }

    const qrCodes = await this.qrCodeDetector_.detect(frame);
    if (qrCodes.length > 0) {
      return qrCodes[0].rawValue;
    }
    return null;
  }

  private onActivationCodeChanged_(): void {
    const event = new CustomEvent('activation-code-updated', {
      bubbles: true, composed: true, detail: {
        activationCode: this.validateActivationCode_(this.activationCode) ?
            this.activationCode :
            null,
      },
    });

    this.dispatchEvent(event);
  }

  private clearQrCodeDetectorTimer_(): void {
    assert(!!this.qrCodeDetectorTimer_);
    clearTimeout(this.qrCodeDetectorTimer_);
    this.qrCodeDetectorTimer_ = null;
  }

  /**
   * Checks if |activationCode| matches or partially matches the correct format.
   * Sets |isActivationCodeInvalidFormat_| to true if the format is incorrect.
   * Returns true if |activationCode| is valid and ready to be submitted for
   * installation.
   */
  private validateActivationCode_(activationCode: string): boolean {
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
  }

  private onSwitchCameraButtonPressed_(): void {
    if (this.state_ === PageState.SCANNING_USER_FACING) {
      this.state_ = PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    } else if (this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
    }
    this.startScanning_();
  }

  private onShowErrorChanged_(): void {
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
  }

  private onStateChanged_(): void {
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
      if (this.qrCodeDetectorTimer_) {
        this.clearQrCodeDetectorTimer_();
      }

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
      this.dispatchEvent(new CustomEvent('focus-default-button', {
        bubbles: true,
        composed: true,
      }));
    }

    this.expanded_ = false;
  }

  private onKeyDown_(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      this.dispatchEvent(new CustomEvent('forward-navigation-requested', {
        bubbles: true,
        composed: true,
      }));
    }

    // Prevents barcode detector video from closing if user tabs through
    // window. We should only close barcode detector window if user
    // types in activation code input.
    if (e.key === 'Tab') {
      return;
    }

    this.state_ = PageState.MANUAL_ENTRY;
    e.stopPropagation();
  }

  private isUiElementHidden_(uiElement: UiElement, state: PageState): boolean {
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
  }

  private isUiElementDisabled_(uiElement: UiElement, state: PageState):
      boolean {
    switch (uiElement) {
      case UiElement.SWITCH_CAMERA:
        return state === PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT ||
            state === PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      default:
        return false;
    }
  }

  private getDescription_(): string {
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
  }

  private shouldActivationCodeInputBeInvalid_(state: PageState): boolean {
    if (this.isActivationCodeInvalidFormat_) {
      return true;
    }
    return state === PageState.MANUAL_ENTRY_INSTALL_FAILURE;
  }

  private getInputSubtitle_(): string {
    // Because this string contains '<' and '>' characters, we cannot use i18n
    // methods.
    return loadTimeData.getString('scanQrCodeInputSubtitle');
  }

  private getInputErrorMessage_(): string {
    // Because this string contains '<' and '>' characters, we cannot use i18n
    // methods.
    return loadTimeData.getString('scanQrCodeInputError');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ActivationCodePageElement.is]: ActivationCodePageElement;
  }
}

customElements.define(ActivationCodePageElement.is, ActivationCodePageElement);
