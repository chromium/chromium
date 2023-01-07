// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element for displaying paired Bluetooth devices.
 */

import '../../settings_shared.css.js';
import './os_paired_bluetooth_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {PairedBluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 */
const SettingsPairedBluetoothListElementBase =
    mixinBehaviors([CrScrollableBehavior], PolymerElement);

/** @polymer */
class SettingsPairedBluetoothListElement extends
    SettingsPairedBluetoothListElementBase {
  static get is() {
    return 'os-settings-paired-bluetooth-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {!Array<!PairedBluetoothDeviceProperties>}
       */
      devices: {
        type: Array,
        observer: 'onDevicesChanged_',
        value: [],
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       * @private
       */
      lastFocused_: Object,
    };
  }

  constructor() {
    super();
    /**
     * The address of the device corresponding to the tooltip if it is currently
     * showing. If undefined, the tooltip is not showing.
     * @type {string|undefined}
     * @private
     */
    this.currentTooltipDeviceAddress_;
  }

  /** @private */
  onDevicesChanged_() {
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
   * @param {!{detail: {address: string, show: boolean, element: ?HTMLElement}}}
   *     e
   * @private
   */
  onManagedTooltipStateChange_(e) {
    const target = e.detail.element;
    const hide = () => {
      /** @type {{hide: Function}} */ (this.$.tooltip).hide();
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

    // paper-tooltip normally determines the target from the |for| property,
    // which is a selector. Here paper-tooltip is being reused by multiple
    // potential targets. Since paper-tooltip does not expose a public property
    // or method to update the target, the private property |_target| is
    // updated directly.
    this.$.tooltip._target = target;
    /** @type {{updatePosition: Function}} */ (this.$.tooltip).updatePosition();
    target.addEventListener('mouseleave', hide);
    target.addEventListener('blur', hide);
    target.addEventListener('tap', hide);
    this.$.tooltip.addEventListener('mouseenter', hide);
    this.$.tooltip.show();
    this.currentTooltipDeviceAddress_ = e.detail.address;
  }
}

customElements.define(
    SettingsPairedBluetoothListElement.is, SettingsPairedBluetoothListElement);
