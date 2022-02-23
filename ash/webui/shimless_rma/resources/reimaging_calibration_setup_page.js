// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationSetupInstruction, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

const instructionMessagesKeys = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'calibrateBaseInstructionsText',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'calibrateLidInstructionsText'
};

/**
 * @fileoverview
 * 'reimaging-calibration-setup-page' is for displaying instructions for the
 * user to prepare the device for a calibration step.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingCalibrationSetupPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingCalibrationSetupPage extends
    ReimagingCalibrationSetupPageBase {
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
          this.calibrationInstructions_ =
              this.i18n(instructionMessagesKeys[result.instructions]);
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
}

customElements.define(
    ReimagingCalibrationSetupPage.is, ReimagingCalibrationSetupPage);
