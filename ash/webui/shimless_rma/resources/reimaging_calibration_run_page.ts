// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_calibration_run_page.html.js';
import {CalibrationComponentStatus, CalibrationObserverReceiver, CalibrationOverallStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-calibration-page' is for recalibration of the
 * various components during the reimaging process.
 */

const ReimagingCalibrationRunPageBase = I18nMixin(PolymerElement);

export class ReimagingCalibrationRunPage extends
    ReimagingCalibrationRunPageBase {
  static get is() {
    return 'reimaging-calibration-run-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      calibrationComplete: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();
  private calibrationObserverReceiver: CalibrationObserverReceiver;
  protected calibrationComplete: boolean;

  constructor() {
    super();
    this.calibrationObserverReceiver = new CalibrationObserverReceiver(this);

    this.shimlessRmaService.observeCalibrationProgress(
        this.calibrationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    if (this.calibrationComplete) {
      return this.shimlessRmaService.calibrationComplete();
    }
    return Promise.reject(new Error('Calibration is not complete.'));
  }

  /**
   * Implements CalibrationObserver.onCalibrationUpdated()
   */
  onCalibrationUpdated(_componentStatus: CalibrationComponentStatus): void {}

  /**
   * Implements CalibrationObserver.onCalibrationStepComplete()
   */
  onCalibrationStepComplete(status: CalibrationOverallStatus): void {
    switch (status) {
      case CalibrationOverallStatus.kCalibrationOverallComplete:
        this.calibrationComplete = true;
        enableNextButton(this);
        break;
      case CalibrationOverallStatus.kCalibrationOverallCurrentRoundComplete:
      case CalibrationOverallStatus.kCalibrationOverallCurrentRoundFailed:
      case CalibrationOverallStatus.kCalibrationOverallInitializationFailed:
        executeThenTransitionState(
            this, () => this.shimlessRmaService.continueCalibration());
        break;
    }
  }

  protected getCalibrationTitleString(): string {
    return this.i18n(
        this.calibrationComplete ? 'runCalibrationCompleteTitleText' :
                                   'runCalibrationTitleText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReimagingCalibrationRunPage.is]: ReimagingCalibrationRunPage;
  }
}

customElements.define(
    ReimagingCalibrationRunPage.is, ReimagingCalibrationRunPage);
