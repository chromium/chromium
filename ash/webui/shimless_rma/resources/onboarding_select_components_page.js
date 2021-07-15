// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Component, ComponentRepairState, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

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
  [ComponentType.kKeyboard]: 'Keyboard',
  [ComponentType.kScreen]: 'Screen',
  [ComponentType.kTrackpad]: 'Trackpad',
  [ComponentType.kPowerButton]: 'Power Button',
  [ComponentType.kThumbReader]: 'Thumb Reader'
};

/**
 * @type {!Object<!ComponentType, string>}
 */
const ComponentTypeToId = {
  [ComponentType.kKeyboard]: 'componentKeyboard',
  [ComponentType.kScreen]: 'componentScreen',
  [ComponentType.kTrackpad]: 'componentTrackpad',
  [ComponentType.kPowerButton]: 'componentPowerButton',
  [ComponentType.kThumbReader]: 'componentThumbReader'
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
          checked: item.state === ComponentRepairState.kReplaced,
          disabled: item.state === ComponentRepairState.kMissing
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
      /** @type {!ComponentRepairState} */
      let state = ComponentRepairState.kOriginal;
      if (item.disabled) {
        state = ComponentRepairState.kMissing;
      } else if (item.checked) {
        state = ComponentRepairState.kReplaced;
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
