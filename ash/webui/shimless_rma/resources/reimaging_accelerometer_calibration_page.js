// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponent, CalibrationObserverInterface, CalibrationObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-accelerometer-calibration-page' is for recalibration of the
 * accelerometer during the reimaging process.
 * TODO(joonbug): when needed, generalize this for different components.
 */
export class ReimagingAccelerometerCalibrationPageElement extends
    PolymerElement {
  static get is() {
    return 'reimaging-accelerometer-calibration-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: null,
      },

      /**
       * Receiver responsible for observing battery health.
       * @protected {CalibrationObserverReceiver}
       */
      calibrationObserverReceiver_: {
        type: Object,
        value: null,
      },

      /** @protected */
      calibrationComplete_: {
        type: Boolean,
        value: false,
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
  }


  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (!this.calibrationObserverReceiver_) {
      this.observeCalibrationProgress_();
      return Promise.reject();
    }

    if (this.calibrationComplete_) {
      return this.shimlessRmaService_.transitionNextState();
    }
    return Promise.reject(new Error('Calibration is not complete.'));
  }

  /**
   * Implements ProvisioningObserver.onProvisioningUpdated()
   * @param {!CalibrationComponent} component
   * @param {number} progress
   */
  onCalibrationUpdated(component, progress) {
    if (progress === 100) {
      this.calibrationComplete_ = true;
    }
  }

  /** @private */
  observeCalibrationProgress_() {
    this.calibrationObserverReceiver_ = new CalibrationObserverReceiver(
        /** @type {!CalibrationObserverInterface} */ (this));

    this.shimlessRmaService_.observeCalibrationProgress(
        this.calibrationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }
};

customElements.define(
    ReimagingAccelerometerCalibrationPageElement.is,
    ReimagingAccelerometerCalibrationPageElement);
