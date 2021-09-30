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
import {CalibrationSetupInstruction, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// TODO(gavindodd): i18n string
const instructionMessages = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'Please place the device on a flat surface before proceeding.',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'Please place the lid of the device on a flat surface before proceeding.'
};

/**
 * @fileoverview
 * 'reimaging-calibration-setup-page' is for displaying instructions for the
 * user to prepare the device for a calibration step.
 */
export class ReimagingCalibrationSetupPageElement extends PolymerElement {
  static get is() {
    return 'reimaging-calibration-setup-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      calibrationInstructions_: {
        type: String,
        value: '',
      }
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_.getCalibrationSetupInstructions().then(
        (result) => {
          let message = instructionMessages[result.instructions];
          if (message === undefined) {
            // This is a catchall in case of errors.
            // TODO(gavindodd): i18n string
            this.calibrationInstructions_ = 'Click Next to proceed';
          } else {
            this.calibrationInstructions_ = message;
          }
        });
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.runCalibrationStep();
  }
};

customElements.define(
    ReimagingCalibrationSetupPageElement.is,
    ReimagingCalibrationSetupPageElement);
