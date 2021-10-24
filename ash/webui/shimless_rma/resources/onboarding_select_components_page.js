// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId, ComponentTypeToName} from './data.js';
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
      /** @protected {!Array<!ComponentCheckbox>} */
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
    this.getComponents_();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /** @private */
  getComponents_() {
    this.shimlessRmaService_.getComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      const componentList = [];
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
    this.dispatchEvent(new CustomEvent(
        'transition-state',
        {
          bubbles: true,
          composed: true,
          detail: (() => {
            return this.shimlessRmaService_.reworkMainboard();
          })
        },
        ));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.setComponentList(
        this.getComponentRepairStateList_());
  }
}

customElements.define(
    OnboardingSelectComponentsPageElement.is,
    OnboardingSelectComponentsPageElement);
