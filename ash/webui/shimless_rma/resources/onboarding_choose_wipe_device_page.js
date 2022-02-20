// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';

import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'onboarding-choose-wipe-device-page' allows user to select between wiping all
 * the device data at the end of the RMA process or preserving it.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingChooseWipeDevicePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingChooseWipeDevicePage extends
    OnboardingChooseWipeDevicePageBase {
  static get is() {
    return 'onboarding-choose-wipe-device-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    OnboardingChooseWipeDevicePage.is, OnboardingChooseWipeDevicePage);
