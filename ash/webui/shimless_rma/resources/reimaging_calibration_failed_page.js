// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './base_page.js';
import './calibration_component_chip.js';
import './icons.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {enableNextButton, executeThenTransitionState} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-calibration-failed-page' is to inform the user which components
 * will be calibrated and allow them to skip components if necessary.
 * (Skipping components could allow the device to be in a usable, but not fully
 * functioning state.)
 */

/**
 * @typedef {{
 *   component: !ComponentType,
 *   id: string,
 *   name: string,
 *   checked: boolean,
 *   failed: boolean,
 * }}
 */
let ComponentCheckbox;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingCalibrationFailedPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingCalibrationFailedPage extends
    ReimagingCalibrationFailedPageBase {
  static get is() {
    return 'reimaging-calibration-failed-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @private {!Array<!ComponentCheckbox>} */
      componentCheckboxes_: {
        type: Array,
        value: () => [],
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();

    /**
     * The "Skip calibration" button on this page is styled and positioned like
     * a cancel button. So we use the common cancel button from shimless_rma.js
     * This function needs to be public, because it's invoked by
     * shimless_rma.js as part of the response to the cancel button click.
     * @return {!Promise<!StateResult>}
     */
    this.onCancelButtonClick = () => {
      if (this.tryingToSkipWithFailedComponents_()) {
        this.shadowRoot.querySelector('#failedComponentsDialog').showModal();
        return Promise.reject(
            new Error('Attempting to skip with failed components.'));
      }

      return this.skipCalibration_();
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.getInitialComponentsList_();
    enableNextButton(this);
  }

  /** @private */
  getInitialComponentsList_() {
    this.shimlessRmaService_.getCalibrationComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      this.componentCheckboxes_ = result.components.map(item => {
        return {
          component: item.component,
          id: ComponentTypeToId[item.component],
          name: this.i18n(ComponentTypeToId[item.component]),
          checked: false,
          failed: item.status === CalibrationStatus.kCalibrationFailed,
          // Disable components that did not fail calibration so they can't be
          // selected for calibration again.
          disabled: item.status !== CalibrationStatus.kCalibrationFailed,
        };
      });
    });
  }

  /**
   * @return {!Array<!CalibrationComponentStatus>}
   * @private
   */
  getComponentsList_() {
    return this.componentCheckboxes_.map(item => {
      return {
        component: item.component,
        status: item.checked ? CalibrationStatus.kCalibrationWaiting :
                               CalibrationStatus.kCalibrationSkip,
        progress: 0.0
      };
    });
  }

  /**
   * @return {!Promise<!StateResult>}
   * @private
   */
  skipCalibration_() {
    const skippedComponents = this.componentCheckboxes_.map(item => {
      return {
        component: item.component,
        status: CalibrationStatus.kCalibrationSkip,
        progress: 0.0
      };
    });
    return this.shimlessRmaService_.startCalibration(skippedComponents);
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.startCalibration(this.getComponentsList_());
  }

  /**
   * @param {boolean} componentDisabled
   * @return {boolean}
   * @private
   */
  isComponentDisabled_(componentDisabled) {
    return componentDisabled || this.allButtonsDisabled;
  }

  /** @protected */
  onSkipDialogButtonClicked_() {
    this.closeDialog_();
    executeThenTransitionState(this, () => this.skipCalibration_());
  }

  /** @protected */
  closeDialog_() {
    this.shadowRoot.querySelector('#failedComponentsDialog').close();
  }

  /**
   * @return {boolean}
   * @private
   */
  tryingToSkipWithFailedComponents_() {
    return this.componentCheckboxes_.some(
        component => component.failed && !component.checked);
  }
}

customElements.define(
    ReimagingCalibrationFailedPage.is, ReimagingCalibrationFailedPage);
