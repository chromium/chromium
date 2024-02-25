// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying paired Bluetooth devices.
 */

import '../settings_shared.css.js';
import './os_paired_bluetooth_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableMixin} from 'chrome://resources/ash/common/cr_elements/cr_scrollable_mixin.js';
import {PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_paired_bluetooth_list.html.js';

export interface SettingsPairedBluetoothListElement {
  $: {
    tooltip: PaperTooltipElement,
  };
}

const SettingsPairedBluetoothListElementBase =
    CrScrollableMixin(PolymerElement);

export class SettingsPairedBluetoothListElement extends
    SettingsPairedBluetoothListElementBase {
  static get is() {
    return 'os-settings-paired-bluetooth-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      devices: {
        type: Array,
        observer: 'onDevicesChanged_',
        value: [],
      },

      /**
       * Used by FocusRowMixin in <os-settings-paired-bluetooth-list-item>
       * to track the last focused element on a row.
       */
      lastFocused_: Object,
    };
  }

  devices: PairedBluetoothDeviceProperties[];
  private currentTooltipDeviceAddress_: string|undefined;
  private lastFocused_: HTMLElement;

  constructor() {
    super();

    /**
     * The address of the device corresponding to the tooltip if it is currently
     * showing. If undefined, the tooltip is not showing.
     */
    this.currentTooltipDeviceAddress_;
  }

  private onDevicesChanged_(): void {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates.
    this.updateScrollableContents();
  }

  /**
   * Updates the visibility of the enterprise policy UI tooltip. This is
   * triggered by the managed-tooltip-state-change event. This event can be
   * fired in two cases:
   * 1) We want to show the tooltip for a given device's icon. Here, show will
   *    be true and the element will be defined.
   * 2) We want to make sure there is no tooltip showing for a given device's
   *    icon. Here, show will be false and the element undefined.
   * In both cases, address will be the item's device address.
   * We need to use a common tooltip since a tooltip within the item gets cut
   * off from the iron-list.
   */
  private onManagedTooltipStateChange_(
      e: CustomEvent<
          {address: string, show: boolean, element: HTMLElement|null}>): void {
    const target = e.detail.element;
    const hide = (): void => {
      this.$.tooltip.hide();
      this.$.tooltip.removeEventListener('mouseenter', hide);
      this.currentTooltipDeviceAddress_ = undefined;
      if (target) {
        target.removeEventListener('mouseleave', hide);
        target.removeEventListener('blur', hide);
        target.removeEventListener('tap', hide);
      }
    };

    if (!e.detail.show) {
      if (this.currentTooltipDeviceAddress_ &&
          e.detail.address === this.currentTooltipDeviceAddress_) {
        hide();
      }
      return;
    }

    this.$.tooltip.target = target;
    this.$.tooltip.updatePosition();
    target!.addEventListener('mouseleave', hide);
    target!.addEventListener('blur', hide);
    target!.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
    this.currentTooltipDeviceAddress_ = e.detail.address;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPairedBluetoothListElement.is]: SettingsPairedBluetoothListElement;
  }
}

customElements.define(
    SettingsPairedBluetoothListElement.is, SettingsPairedBluetoothListElement);
