// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './base_page.js';
import './calibration_component_chip.js';
import './icons.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId, ComponentTypeToName} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-calibration-page' is to inform the user which components will be
 * calibrated and allow them to skip components if necessary.
 * (Skipping components could allow the device to be in a usable, but not fully
 * functioning state.)
 */

/**
 * @typedef {{
 *   component: !ComponentType,
 *   id: string,
 *   name: string,
 *   skip: boolean,
 *   completed: boolean,
 *   failed: boolean,
 *   disabled: boolean
 * }}
 */
let ComponentCheckbox;

export class ReimagingCalibrationPageElement extends PolymerElement {
  static get is() {
    return 'reimaging-calibration-page';
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

      /** @type {!Array<!ComponentCheckbox>} */
      const componentList = [];
      result.components.forEach(item => {
        const component = assert(item.component);

        componentList.push({
          component: item.component,
          id: ComponentTypeToId[item.component],
          name: ComponentTypeToName[item.component],
          skip: item.status === CalibrationStatus.kCalibrationSkip,
          completed: item.status === CalibrationStatus.kCalibrationComplete,
          failed: item.status === CalibrationStatus.kCalibrationFailed,
          disabled: item.status === CalibrationStatus.kCalibrationComplete ||
              item.status === CalibrationStatus.kCalibrationInProgress
        });
      });
      this.componentCheckboxes_ = componentList;
    });
  }

  /**
   * @private
   * @return {!Array<!CalibrationComponentStatus>}
   */
  getComponentsList_() {
    return this.componentCheckboxes_.map(item => {
      /** @type {!CalibrationStatus} */
      let status = CalibrationStatus.kCalibrationWaiting;
      if (item.skip) {
        status = CalibrationStatus.kCalibrationSkip;
      } else if (item.completed) {
        status = CalibrationStatus.kCalibrationComplete;
      } else if (item.disabled) {
        status = CalibrationStatus.kCalibrationInProgress;
      }
      return {component: item.component, status: status, progress: 0.0};
    });
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.startCalibration(this.getComponentsList_());
  }
}

customElements.define(
    ReimagingCalibrationPageElement.is, ReimagingCalibrationPageElement);
