// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-wait-for-manual-wp-disable-page' wait for the manual HWWP disable
 * to be completed.
 */
export class OnboardingWaitForManualWpDisablePageElement extends
    PolymerElement {
  static get is() {
    return 'onboarding-wait-for-manual-wp-disable-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {ShimlessRmaServiceInterface} */
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
    // TODO(gavindodd): observe HWWP state with
    // observeHardwareWriteProtectionState and control next button enabled from
    // it.
  }
};

customElements.define(
    OnboardingWaitForManualWpDisablePageElement.is,
    OnboardingWaitForManualWpDisablePageElement);
