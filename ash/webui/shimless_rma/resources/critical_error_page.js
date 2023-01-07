// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';
import {disableAllButtons, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'critical-error-page' is displayed when an unexpected error blocks RMA from
 * continuing.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const CriticalErrorPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class CriticalErrorPage extends CriticalErrorPageBase {
  static get is() {
    return 'critical-error-page';
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
  onExitToLoginButtonClicked_() {
    this.shimlessRmaService_.criticalErrorExitToLogin();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }

  /** @protected */
  onRebootButtonClicked_() {
    this.shimlessRmaService_.criticalErrorReboot();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }
}

customElements.define(CriticalErrorPage.is, CriticalErrorPage);
