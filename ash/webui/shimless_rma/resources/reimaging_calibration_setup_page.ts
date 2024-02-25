// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_calibration_setup_page.html.js';
import {CalibrationSetupInstruction, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';

const INSRUCTION_MESSAGE_KEY_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'calibrateBaseInstructionsText',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'calibrateLidInstructionsText',
};

const CALIBRATION_IMG_MAP = {
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceBaseOnFlatSurface]:
      'base_on_flat_surface',
  [CalibrationSetupInstruction.kCalibrationInstructionPlaceLidOnFlatSurface]:
      'lid_on_flat_surface',
};

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

const ReimagingCalibrationSetupPageBase = I18nMixin(PolymerElement);

export class ReimagingCalibrationSetupPage extends
    ReimagingCalibrationSetupPageBase {
  static get is() {
    return 'reimaging-calibration-setup-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      calibrationSetupInstruction: {
        type: Object,
      },

      imgSrc: {
        type: String,
        value: '',
      },

      imgAlt: {
        type: String,
        value: '',
      },

      calibrationInstructionsText: {
        type: String,
      },
    };
  }

  calibrationSetupInstruction: CalibrationSetupInstruction;
  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  protected imgSrc: string;
  protected imgAlt: string;
  protected calibrationInstructionsText: string;

  static get observers() {
    return ['onStatusChanged(calibrationSetupInstruction)'];
  }

  override ready() {
    super.ready();
    this.shimlessRmaService.getCalibrationSetupInstructions().then(
        (result: {instructions: CalibrationSetupInstruction}) => {
          this.calibrationSetupInstruction = result.instructions;
        });
    enableNextButton(this);

    focusPageTitle(this);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.runCalibrationStep();
  }

  /**
   * Groups state changes related to the |calibrationSetupInstruction|
   * updating.
   */
  protected onStatusChanged(): void {
    this.setCalibrationInstructionsText();
    this.setImgSrcAndAlt();
  }

  protected setCalibrationInstructionsText(): void {
    assert(this.calibrationSetupInstruction);
    this.calibrationInstructionsText =
        this.i18n(INSRUCTION_MESSAGE_KEY_MAP[this.calibrationSetupInstruction]);
  }

  protected setImgSrcAndAlt(): void {
    assert(this.calibrationSetupInstruction);
    this.imgSrc = `illustrations/${
        CALIBRATION_IMG_MAP[this.calibrationSetupInstruction]}.svg`;
    this.imgAlt =
        this.i18n(CALIBRATION_ALT_MAP[this.calibrationSetupInstruction]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReimagingCalibrationSetupPage.is]: ReimagingCalibrationSetupPage;
  }
}

customElements.define(
    ReimagingCalibrationSetupPage.is, ReimagingCalibrationSetupPage);
