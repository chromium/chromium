// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../os_settings_icons.html.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExhaustive} from '../assert_extras.js';

import type {DevicePageBrowserProxy} from './device_page_browser_proxy.js';
import {DevicePageBrowserProxyImpl, OptimizedChargingStrategy} from './device_page_browser_proxy.js';
import {SettingsPowerElement} from './power.js';
import {getTemplate} from './power_optimized_charging_dialog.html.js';

export interface PowerOptimizedChargingDialogElement {
  $: {
    dialog: CrDialogElement,
    radioGroup: CrRadioGroupElement,
  };
}

/**
 * Names of the radio buttons which allow the user to choose their optimized
 * charging strategy.
 */
enum OptimizedChargingButtons {
  ADAPTIVE_CHARGING = 'adaptive-charging',
  CHARGE_LIMIT = 'charge-limit',
}

const PowerOptimizedChargingDialogElementBase = PrefsMixin(PolymerElement);

export class PowerOptimizedChargingDialogElement extends
    PowerOptimizedChargingDialogElementBase {
  static get is() {
    return 'power-optimized-charging-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      previousStrategy: {type: Number, computed: `getPreviousPrefValue_()`},
      selectedOption_: {
        type: String,
        computed: 'computeButtonName_(previousStrategy)',
      },
    };
  }

  private browserProxy_: DevicePageBrowserProxy;
  private selectedOption_: OptimizedChargingButtons;

  previousStrategy: OptimizedChargingStrategy;

  constructor() {
    super();

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  private onDoneClick_(): void {
    const uiSelectedButton: OptimizedChargingButtons =
        this.$.radioGroup.selected as OptimizedChargingButtons;
    const newlySelectedStrategy: OptimizedChargingStrategy =
        this.convertRadioNameToStrategyEnum_(uiSelectedButton);


    if (this.previousStrategy !== newlySelectedStrategy) {
      this.setPrefValue(
          SettingsPowerElement.OPTIMIZED_CHARGING_STRATEGY_PREF_NAME,
          newlySelectedStrategy);
    }
    this.$.dialog.close();
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  private computeButtonName_(strategy: OptimizedChargingStrategy):
      OptimizedChargingButtons {
    switch (strategy) {
      case OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING:
        return OptimizedChargingButtons.ADAPTIVE_CHARGING;
      case OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT:
        return OptimizedChargingButtons.CHARGE_LIMIT;
      default:
        assertExhaustive(strategy);
    }
  }

  private getPreviousPrefValue_() {
    return this.getPref<number>(
                   SettingsPowerElement.OPTIMIZED_CHARGING_STRATEGY_PREF_NAME)
               .value as OptimizedChargingStrategy;
  }

  private convertRadioNameToStrategyEnum_(
      radioButtonName: OptimizedChargingButtons): OptimizedChargingStrategy {
    switch (radioButtonName) {
      case OptimizedChargingButtons.ADAPTIVE_CHARGING:
        return OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING;
      case OptimizedChargingButtons.CHARGE_LIMIT:
        return OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT;
      default:
        assertExhaustive(radioButtonName);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PowerOptimizedChargingDialogElement.is]:
        PowerOptimizedChargingDialogElement;
  }
}

customElements.define(
    PowerOptimizedChargingDialogElement.is,
    PowerOptimizedChargingDialogElement);
