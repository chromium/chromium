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
  }

  /** @override */
  ready() {
    super.ready();
    this.getInitialComponentsList_();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
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
        };
      });
    });
  }

  /**
   * @private
   * @return {!Array<!CalibrationComponentStatus>}
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

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.skipCalibration_();
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

  /** @private */
  onRetryCalibrationButtonClicked_() {
    this.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: (() => {
            return this.shimlessRmaService_.startCalibration(
                this.getComponentsList_());
          })
        },
        ));
  }
}

customElements.define(
    ReimagingCalibrationFailedPage.is, ReimagingCalibrationFailedPage);
