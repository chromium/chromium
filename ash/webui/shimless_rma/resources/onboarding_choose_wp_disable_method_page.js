// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_choose_wp_disable_method_page.html.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {disableNextButton, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-choose-wp-disable-method-page' allows user to select between
 * hardware or RSU write protection disable methods.
 *
 * TODO(gavindodd): Change "Manual" description based on enterprise enrollment
 * status.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingChooseWpDisableMethodPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingChooseWpDisableMethodPage extends
    OnboardingChooseWpDisableMethodPageBase {
  static get is() {
    return 'onboarding-choose-wp-disable-method-page';
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

      /** @private */
      hwwpMethod: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @protected
   */
  onHwwpDisableMethodSelectionChanged(event) {
    this.hwwpMethod = event.detail.value;
    const disabled = !this.hwwpMethod;
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (this.hwwpMethod === 'hwwpDisableMethodManual') {
      return this.shimlessRmaService.chooseManuallyDisableWriteProtect();
    } else if (this.hwwpMethod === 'hwwpDisableMethodRsu') {
      return this.shimlessRmaService.chooseRsuDisableWriteProtect();
    } else {
      return Promise.reject(new Error('No disable method selected'));
    }
  }
}

customElements.define(
    OnboardingChooseWpDisableMethodPage.is,
    OnboardingChooseWpDisableMethodPage);
