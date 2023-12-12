// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';
import {getTemplate} from './wrapup_restock_page.html.js';

/**
 * @fileoverview
 * 'wrapup-restock-page' is the page that informs the repair technician they
 * can shut down the device and restock the mainboard or continue to finalize
 * the repair if the board is being used to repair another device.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const WrapupRestockPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class WrapupRestockPage extends WrapupRestockPageBase {
  static get is() {
    return 'wrapup-restock-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,
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

    focusPageTitle(this);
  }

  /** @protected */
  onShutdownButtonClicked_() {
    executeThenTransitionState(
        this, () => this.shimlessRmaService_.shutdownForRestock());
  }

  /** @protected */
  onRestockContinueButtonClicked_() {
    executeThenTransitionState(
        this,
        () => this.shimlessRmaService_.continueFinalizationAfterRestock());
  }
}

customElements.define(WrapupRestockPage.is, WrapupRestockPage);
