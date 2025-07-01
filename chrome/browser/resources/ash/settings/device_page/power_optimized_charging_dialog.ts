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

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_optimized_charging_dialog.html.js';

export interface PowerOptimizedChargingDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}


export class PowerOptimizedChargingDialogElement extends PolymerElement {
  static get is() {
    return 'power-optimized-charging-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onDoneClick_(): void {
    // TODO(crbug.com/428255416): Save temporary strategy choice to pref.
    this.$.dialog.close();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onChargingStrategyRadioSelectionChanged_(): void {
    // TODO(crbug.com/428255416): Update radio button choice to temporary
    // strategy.
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
