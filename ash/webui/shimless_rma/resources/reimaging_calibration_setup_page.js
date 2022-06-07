// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationSetupInstruction, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {enableNextButton} from './shimless_rma_util.js';

/** @type {!Object<!CalibrationSetupInstruction, string>} */
const INSRUCTION_MESSAGE_KEY_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'calibrateBaseInstructionsText',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'calibrateLidInstructionsText'
};

/** @type {!Object<!CalibrationSetupInstruction, string>} */
const CALIBRATION_IMG_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'base_on_flat_surface',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'lid_on_flat_surface',
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
      /** @protected {?CalibrationSetupInstruction} */
      calibrationSetupInstruction_: {
        type: Object,
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
          this.calibrationSetupInstruction_ = result.instructions;
        });
    enableNextButton(this);
  }

  /** @return {!Promise<{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.runCalibrationStep();
  }

  /**
   * @return {string}
   * @protected
   */
  getCalibrationInstructionsText_() {
    assert(this.calibrationSetupInstruction_);
    return this.i18n(
        INSRUCTION_MESSAGE_KEY_MAP[this.calibrationSetupInstruction_]);
  }

  /**
   * @return {string}
   * @protected
   */
  getImgSrc_() {
    assert(this.calibrationSetupInstruction_);
    return `illustrations/${
        CALIBRATION_IMG_MAP[this.calibrationSetupInstruction_]}.svg`;
  }
}

customElements.define(
    ReimagingCalibrationSetupPage.is, ReimagingCalibrationSetupPage);
