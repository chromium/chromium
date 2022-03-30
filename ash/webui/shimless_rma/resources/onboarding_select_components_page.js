// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Component, ComponentRepairStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {enableNextButton, executeThenTransitionState} from './shimless_rma_util.js';

/**
 * @typedef {{
 *   component: !ComponentType,
 *   id: string,
 *   identifier: string,
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

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingSelectComponentsPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingSelectComponentsPageElement extends
    OnboardingSelectComponentsPageElementBase {
  static get is() {
    return 'onboarding-select-components-page';
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

      /** @protected {!Array<!ComponentCheckbox>} */
      componentCheckboxes_: {
        type: Array,
        value: () => [],
      },

      /** @private {string} */
      reworkFlowLinkText_: {type: String, value: ''},
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
    this.setReworkFlowLink_();
    this.getComponents_();
    enableNextButton(this);
  }

  /** @private */
  getComponents_() {
    this.shimlessRmaService_.getComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      this.componentCheckboxes_ = result.components.map(item => {
        assert(item.component);
        return {
          component: item.component,
          id: ComponentTypeToId[item.component],
          identifier: item.identifier,
          name: this.i18n(ComponentTypeToId[item.component]),
          checked: item.state === ComponentRepairStatus.kReplaced,
          disabled: item.state === ComponentRepairStatus.kMissing
        };
      });
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
      return {
        component: item.component,
        state: state,
        identifier: item.identifier,
      };
    });
  }

  /** @protected */
  onReworkFlowLinkClicked_(e) {
    e.preventDefault();
    executeThenTransitionState(
        this, () => this.shimlessRmaService_.reworkMainboard());
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.setComponentList(
        this.getComponentRepairStateList_());
  }

  /** @protected */
  setReworkFlowLink_() {
    this.reworkFlowLinkText_ =
        this.i18nAdvanced('reworkFlowLinkText', {attrs: ['id']});
    const linkElement = this.shadowRoot.querySelector('#reworkFlowLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', e => {
      if (this.allButtonsDisabled) {
        return;
      }

      this.onReworkFlowLinkClicked_(e);
    });
  }

  /**
   * @param {boolean} componentDisabled
   * @return {boolean}
   * @protected
   */
  isComponentDisabled_(componentDisabled) {
    return this.allButtonsDisabled || componentDisabled;
  }
}

customElements.define(
    OnboardingSelectComponentsPageElement.is,
    OnboardingSelectComponentsPageElement);
