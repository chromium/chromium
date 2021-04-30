// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './shimless_rma_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
 * // TODO(joonbug): update type to <RmaState, PageInfo> using a Map.
 * @type {!Object<number, !PageInfo>}
 */
const StateComponentMapping = {
  // TODO(joonbug): Update with enum and actual componentId.
  0: {componentIs: 'component1'},
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
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.fetchState_().then((state) => this.loadState_(state));
  }

  /** @private */
  fetchState_() {
    // TODO(joonbug): fetch from fake
    return Promise.resolve(0);
  }

  /** @private */
  loadState_(state) {
    const pageInfo = StateComponentMapping[state];
    this.currentPage_ = pageInfo;
    // TODO(joonbug): Load component
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
    return;
  }

  /** @protected */
  onNextBtnClicked_() {
    // TODO(joonbug): fill with action
    return;
  }

  /** @protected */
  onCancelBtnClicked_() {
    // TODO(joonbug): fill with action
    return;
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
