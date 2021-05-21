// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './onboarding_landing_page.js';
import './onboarding_update_page.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CurrentState, NextState, PrevState, RmadErrorCode, RmaState, ShimlessRmaServiceInterface} from './shimless_rma_types.js'

/**
 * Enum for button states.
 * @enum {string}
 */
const BtnState = {
  VISIBLE: 'visible',
  DISABLED: 'disable',
  HIDDEN: 'hidden'
};

/**
 * @typedef {{
 *  componentIs: string,
 *  btnNext: !BtnState,
 *  btnNextLabel: string,
 *  btnCancel: !BtnState,
 *  btnBack: !BtnState,
 * }}
 */
let PageInfo;

/**
 * @type {!Object<!RmaState, !PageInfo>}
 */
const StateComponentMapping = {
  [RmaState.kUnknown]: {
    componentIs: 'badcomponent',
    btnNext: BtnState.HIDDEN,
    btnCancel: BtnState.VISIBLE,
    btnBack: BtnState.HIDDEN,
  },
  [RmaState.kWelcomeScreen]: {
    componentIs: 'onboarding-landing-page',
    btnNext: BtnState.VISIBLE,
    btnCancel: BtnState.VISIBLE,
    btnBack: BtnState.HIDDEN,
  },
  [RmaState.kSelectComponents]: {
    componentIs: 'onboarding-update-page',
    btnNext: BtnState.HIDDEN,
    btnCancel: BtnState.VISIBLE,
    btnBack: BtnState.VISIBLE,
  },
};

/**
 * @fileoverview
 * 'shimless-rma' is the main page for the shimless rma process modal dialog.
 */
export class ShimlessRmaElement extends PolymerElement {
  static get is() {
    return 'shimless-rma';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Current PageInfo based on current state
       * @protected
       * @type {PageInfo}
       */
      currentPage_: {
        type: Object,
        value: {},
      },

      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },

      /**
       * Initial state to cancel to
       * @private {?RmaState}
       */
      initialState_: {
        type: Object,
        value: null,
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();

    // Get the initial state.
    this.fetchState_().then((state) => {
      this.initialState_ = state.currentState;
      this.loadState_(state.currentState);
    });
  }

  /** @private */
  fetchState_() {
    return this.shimlessRmaService_.getCurrentState();
  }

  /** @private */
  fetchNextState_() {
    return this.shimlessRmaService_.getNextState();
  }

  /** @private */
  fetchPrevState_() {
    return this.shimlessRmaService_.getPrevState();
  }

  /**
   * @private
   * @param {!RmaState} state
   */
  loadState_(state) {
    const pageInfo = StateComponentMapping[state];
    assert(pageInfo);

    this.currentPage_ = pageInfo;
    this.showComponent_(pageInfo.componentIs);
  }

  /**
   * @param {string} componentIs
   * @private
   */
  showComponent_(componentIs) {
    let component = this.shadowRoot.querySelector(`#${componentIs}`);
    if (component === null) {
      component = this.loadComponent_(componentIs);
    }

    this.hideAllComponents_();
    component.hidden = false;
  }

  /**
   * Utility method to bulk hide all contents.
   */
  hideAllComponents_() {
    const components = this.shadowRoot.querySelectorAll('.shimless-content');
    Array.from(components).map((c) => c.hidden = true);
  }

  /**
   * @param {string} componentIs
   * @private
   */
  loadComponent_(componentIs) {
    const shimlessBody = this.shadowRoot.querySelector('#contentContainer');

    let component = document.createElement(componentIs);
    component.setAttribute('id', componentIs);
    component.setAttribute('class', 'shimless-content');
    component.hidden = true;

    if (!component) {
      console.error('Failed to create ' + componentIs);
    }

    shimlessBody.appendChild(component);
    return component;
  }

  /** @protected */
  isBtnHidden_(btn) {
    return btn === 'hidden';
  }

  /** @protected */
  isBtnDisabled_(btn) {
    return btn === 'disabled';
  }

  /** @protected */
  onBackBtnClicked_() {
    // TODO(joonbug): error handling based on state.error
    this.fetchPrevState_().then((state) => this.loadState_(state.prevState));
  }

  /** @protected */
  onNextBtnClicked_() {
    // TODO(joonbug): error handling based on state.error
    this.fetchNextState_().then((state) => this.loadState_(state.nextState));
  }

  /** @protected */
  onCancelBtnClicked_() {
    this.loadState_(assert(this.initialState_));
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
