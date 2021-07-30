// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponent, CalibrationObserverInterface, CalibrationObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-calibration-page' is for recalibration of the
 * various components during the reimaging process.
 */
export class ReimagingCalibrationPageElement extends PolymerElement {
  static get is() {
    return 'reimaging-calibration-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {CalibrationComponent} */
      repairedComponent: {
        type: Object,
        value: null,
      },

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
    assert(this.repairedComponent);

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
    if (this.repairedComponent === component && progress === 100) {
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
    ReimagingCalibrationPageElement.is, ReimagingCalibrationPageElement);
