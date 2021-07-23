// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Component, ComponentRepairStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @typedef {{
 *   component: !ComponentType,
 *   id: string,
 *   name: string,
 *   checked: boolean,
 *   disabled: boolean
 * }}
 */
let ComponentCheckbox;

/**
 * @type {!Object<!ComponentType, string>}
 */
const ComponentTypeToName = {
  [ComponentType.kAudioCodec]: 'Audio',
  [ComponentType.kBattery]: 'Battery',
  [ComponentType.kStorage]: 'Storage',
  [ComponentType.kVpdCached]: 'Vpd Cached',
  [ComponentType.kNetwork]: 'Network',
  [ComponentType.kCamera]: 'Camera',
  [ComponentType.kStylus]: 'Stylus',
  [ComponentType.kTouchpad]: 'Touchpad',
  [ComponentType.kTouchsreen]: 'Touchscreen',
  [ComponentType.kDram]: 'Memory',
  [ComponentType.kDisplayPanel]: 'Display',
  [ComponentType.kCellular]: 'Cellular',
  [ComponentType.kEthernet]: 'Ethernet',
  [ComponentType.kWireless]: 'Wireless',
  [ComponentType.kGyroscope]: 'Gyroscope',
  [ComponentType.kAccelerometer]: 'Accelerometer',
  [ComponentType.kScreen]: 'Screen',
  [ComponentType.kKeyboard]: 'Keyboard',
  [ComponentType.kPowerButton]: 'Power Button'
};

/**
 * @type {!Object<!ComponentType, string>}
 */
const ComponentTypeToId = {
  [ComponentType.kAudioCodec]: 'componentAudio',
  [ComponentType.kBattery]: 'componentBattery',
  [ComponentType.kStorage]: 'componentStorage',
  [ComponentType.kVpdCached]: 'componentVpd Cached',
  [ComponentType.kNetwork]: 'componentNetwork',
  [ComponentType.kCamera]: 'componentCamera',
  [ComponentType.kStylus]: 'componentStylus',
  [ComponentType.kTouchpad]: 'componentTouchpad',
  [ComponentType.kTouchsreen]: 'componentTouchscreen',
  [ComponentType.kDram]: 'componentDram',
  [ComponentType.kDisplayPanel]: 'componentDisplayPanel',
  [ComponentType.kCellular]: 'componentCellular',
  [ComponentType.kEthernet]: 'componentEthernet',
  [ComponentType.kWireless]: 'componentWireless',
  [ComponentType.kGyroscope]: 'componentGyroscope',
  [ComponentType.kAccelerometer]: 'componentAccelerometer',
  [ComponentType.kScreen]: 'componentScreen',
  [ComponentType.kKeyboard]: 'componentKeyboard',
  [ComponentType.kPowerButton]: 'componentPowerButton'
};

/**
 * @fileoverview
 * 'onboarding-select-components-page' is the page for selecting the components
 * that were replaced during repair.
 */
export class OnboardingSelectComponentsPageElement extends PolymerElement {
  static get is() {
    return 'onboarding-select-components-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {?ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: null,
      },

      /** @private {!Array<!ComponentCheckbox>} */
      componentCheckboxes_: {
        type: Array,
        value: () => [],
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
    this.getComponents_();
  }

  /** @private */
  getComponents_() {
    this.shimlessRmaService_.getComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      let componentList = [];
      result.components.forEach(item => {
        const component = assert(item.component);

        componentList.push({
          component: item.component,
          id: ComponentTypeToId[item.component],
          name: ComponentTypeToName[item.component],
          checked: item.state === ComponentRepairStatus.kReplaced,
          disabled: item.state === ComponentRepairStatus.kMissing
        });
      });
      this.componentCheckboxes_ = componentList;
    });
  }

  /**
   * @private
   * @return {!Array<!Component>}
   */
  getComponentRepairStateList_() {
    return this.componentCheckboxes_.map(item => {
      /** @type {!ComponentRepairStatus} */
      let state = ComponentRepairStatus.kOriginal;
      if (item.disabled) {
        state = ComponentRepairStatus.kMissing;
      } else if (item.checked) {
        state = ComponentRepairStatus.kReplaced;
      }
      return {component: item.component, state: state};
    });
  }

  /** @protected */
  onReworkFlowButtonClicked_(e) {
    e.preventDefault();
    console.log('Rework flow clicked');
    // TODO(gavindodd): call
    // this.shimlessRmaService_.reworkMainboard().then((state)
    //     => shimlessRma.loadNextState_(state));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.setComponentList(
        this.getComponentRepairStateList_());
  }
};

customElements.define(
    OnboardingSelectComponentsPageElement.is,
    OnboardingSelectComponentsPageElement);
