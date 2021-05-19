// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './onboarding_landing_page.js';
import './onboarding_update_page.js';
import './shimless_rma_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CurrentState, NextState, PrevState, RmadErrorCode, RmaState, ShimlessRmaServiceInterface} from './shimless_rma_types.js'

/**
 * @typedef {{
 *  componentIs: string,
 *  btnNext: string,
 *  btnNextLabel: string,
 *  btnCancel: string,
 *  btnBack: string,
 * }}
 */
let PageInfo;

/**
 * @type {!Object<!RmaState, !PageInfo>}
 */
const StateComponentMapping = {
  [RmaState.kUnknown]: {componentIs: 'badcomponent'},
  [RmaState.kWelcomeScreen]: {componentIs: 'onboarding-landing-page'},
  [RmaState.kUpdateChrome]: {componentIs: 'onboarding-update-page'},
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
      /**
       * @private
       * @type {ShimlessRmaServiceInterface}
       */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();

    // Get the initial state.
    this.fetchState_().then((state) => this.loadState_(state));
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
   * @param { !CurrentState } state
   */
  loadState_(state) {
    const pageInfo = StateComponentMapping[state.currentState];
    this.currentPage_ = pageInfo;
    // TODO(joonbug): Load component
  }

  /**
   * @private
   * @param { !NextState } state
   */
  loadNextState_(state) {
    const pageInfo = StateComponentMapping[state.nextState];
    this.currentPage_ = pageInfo;
    // TODO(joonbug): Load component
  }

  /**
   * @private
   * @param { !PrevState } state
   */
  loadPrevState_(state) {
    const pageInfo = StateComponentMapping[state.prevState];
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
    // TODO(joonbug): fill with action
    this.fetchPrevState_().then((state) => this.loadPrevState_(state));
    return;
  }

  /** @protected */
  onNextBtnClicked_() {
    // TODO(joonbug): fill with action
    this.fetchNextState_().then((state) => this.loadNextState_(state));
    return;
  }

  /** @protected */
  onCancelBtnClicked_() {
    // TODO(joonbug): fill with action
    return;
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
