// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_choose_wipe_device_page.html.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';
import {OnSelectedChangedEvent} from './events.js';

/**
 * @fileoverview
 * 'onboarding-choose-wipe-device-page' allows user to select between wiping all
 * the device data at the end of the RMA process or preserving it.
 */

const OnboardingChooseWipeDevicePageBase = I18nMixin(PolymerElement);

/**
 * Supported options for the wipe device state.
 */
enum WipeDeviceOption {
  WIPE_DEVICE = 'wipeDevice',
  PRESERVE_DATA = 'preserveData',
}

export class OnboardingChooseWipeDevicePage extends
    OnboardingChooseWipeDevicePageBase {
  static get is() {
    return 'onboarding-choose-wipe-device-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Set by shimless_rma.ts.
      allButtonsDisabled: Boolean,

      /**
       * Used to refer to the enum values in HTML file.
       */
      wipeDeviceOption: {
        type: Object,
        value: WipeDeviceOption,
      },
      selectedWipeDeviceOption: {
        type: String,
        value: '',
      },
    };
  }

  allButtonsDisabled: boolean;
  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  protected selectedWipeDeviceOption: string;

  override ready() {
    super.ready();

    focusPageTitle(this);
  }

  protected onOptionChanged(event: OnSelectedChangedEvent): void {
    this.selectedWipeDeviceOption = event.detail.value;

    // Enable the next button when an option is chosen.
    enableNextButton(this);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    assert(!!this.selectedWipeDeviceOption);
    return this.shimlessRmaService.setWipeDevice(
        this.selectedWipeDeviceOption === WipeDeviceOption.WIPE_DEVICE);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingChooseWipeDevicePage.is]: OnboardingChooseWipeDevicePage;
  }
}

customElements.define(
    OnboardingChooseWipeDevicePage.is, OnboardingChooseWipeDevicePage);
