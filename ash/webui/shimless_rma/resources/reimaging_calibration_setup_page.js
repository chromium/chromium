// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_calibration_setup_page.html.js';
import {CalibrationSetupInstruction, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/** @type {!Object<!CalibrationSetupInstruction, string>} */
const INSRUCTION_MESSAGE_KEY_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'calibrateBaseInstructionsText',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'calibrateLidInstructionsText',
};

/** @type {!Object<!CalibrationSetupInstruction, string>} */
const CALIBRATION_IMG_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'base_on_flat_surface',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'lid_on_flat_surface',
};

/** @type {!Object<!CalibrationSetupInstruction, string>} */
const CALIBRATION_ALT_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'baseOnFlatSurfaceAltText',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'lidOnFlatSurfaceAltText',
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
    return getTemplate();
  }

  static get properties() {
    return {
      /** @protected {?CalibrationSetupInstruction} */
      calibrationSetupInstruction: {
        type: Object,
      },

      /** @protected {string} */
      imgSrc: {
        type: String,
        value: '',
      },

      /** @protected {string} */
      imgAlt: {
        type: String,
        value: '',
      },

      /** @protected {string} */
      calibrationInstructionsText: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
  }

  static get observers() {
    return ['onStatusChanged(calibrationSetupInstruction)'];
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService.getCalibrationSetupInstructions().then((result) => {
      this.calibrationSetupInstruction = result.instructions;
    });
    enableNextButton(this);

    focusPageTitle(this);
  }

  /** @return {!Promise<{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService.runCalibrationStep();
  }

  /**
   * Groups state changes related to the |calibrationSetupInstruction|
   * updating.
   * @protected
   */
  onStatusChanged() {
    this.setCalibrationInstructionsText();
    this.setImgSrcAndAlt();
  }

  /**
   * @protected
   */
  setCalibrationInstructionsText() {
    assert(this.calibrationSetupInstruction);
    this.calibrationInstructionsText =
        this.i18n(INSRUCTION_MESSAGE_KEY_MAP[this.calibrationSetupInstruction]);
  }

  /**
   * @protected
   */
  setImgSrcAndAlt() {
    assert(this.calibrationSetupInstruction);
    this.imgSrc = `illustrations/${
        CALIBRATION_IMG_MAP[this.calibrationSetupInstruction]}.svg`;
    this.imgAlt =
        this.i18n(CALIBRATION_ALT_MAP[this.calibrationSetupInstruction]);
  }
}

customElements.define(
    ReimagingCalibrationSetupPage.is, ReimagingCalibrationSetupPage);
