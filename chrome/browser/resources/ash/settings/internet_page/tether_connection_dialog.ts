// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './tether_connection_dialog.html.js';

/**
 * Maps signal strength from [0, 100] to [0, 4] which represents the number
 * of bars in the signal icon displayed to the user. This is used to select
 * the correct icon.
 * @param strength The signal strength from [0 - 100].
 * @return The number of signal bars from [0, 4] as an integer
 */
function signalStrengthToBarCount(strength: number): number {
  if (strength > 75) {
    return 4;
  }
  if (strength > 50) {
    return 3;
  }
  if (strength > 25) {
    return 2;
  }
  if (strength > 0) {
    return 1;
  }
  return 0;
}

const TetherConnectionDialogElementBase = I18nMixin(PolymerElement);

export class TetherConnectionDialogElement extends
    TetherConnectionDialogElementBase {
  static get is() {
    return 'tether-connection-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      managedProperties: Object,

      /**
       * Whether the network has been lost (e.g., has gone out of range).
       */
      outOfRange: Boolean,
    };
  }

  managedProperties: ManagedProperties|undefined;
  outOfRange: boolean;

  open(): void {
    const dialog = this.getDialog_();
    if (!dialog.open) {
      dialog.showModal();
    }

    this.shadowRoot!.querySelector<CrButtonElement>('#connectButton')!.focus();
  }

  close(): void {
    const dialog = this.getDialog_();
    if (dialog.open) {
      dialog.close();
    }
  }

  private getDialog_(): CrDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<CrDialogElement>('#dialog'));
  }

  private onNotNowClick_(): void {
    this.getDialog_().cancel();
  }

  private onConnectClick_(): void {
    const event =
        new CustomEvent('tether-connect', {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }

  private shouldShowDisconnectFromWifi_(_managedProperties: ManagedProperties|
                                        undefined): boolean {
    // TODO(khorimoto): Pipe through a new network property which describes
    // whether the tether host is currently connected to a Wi-Fi network. Return
    // whether it is here.
    return true;
  }

  /**
   * @return The battery percentage integer value converted to a
   *     string. Note that this will not return a string with a "%" suffix.
   */
  private getBatteryPercentageAsString_(managedProperties: ManagedProperties|
                                        undefined): string {
    return managedProperties ?
        managedProperties.typeProperties.tether!.batteryPercentage.toString() :
        '0';
  }

  /**
   * Retrieves an image that corresponds to signal strength of the tether host.
   * Custom icons are used here instead of a <network-icon> because this
   * dialog uses a special color scheme.
   */
  private getSignalStrengthIconName_(managedProperties: ManagedProperties|
                                     undefined): string {
    const signalStrength = managedProperties ?
        managedProperties.typeProperties.tether!.signalStrength :
        0;
    const barCount = signalStrengthToBarCount(signalStrength);
    return `os-settings:signal-cellular-${barCount}-bar`;
  }

  /**
   * Retrieves a localized accessibility label for the signal strength.
   */
  private getSignalStrengthLabel_(managedProperties: ManagedProperties|
                                  undefined): string {
    const signalStrength = managedProperties ?
        managedProperties.typeProperties.tether!.signalStrength :
        0;
    const networkTypeString = this.i18n('OncTypeTether');
    return this.i18n(
        'networkIconLabelSignalStrength', networkTypeString, signalStrength);
  }

  private getDeviceName_(managedProperties: ManagedProperties|
                         undefined): string {
    return managedProperties ? OncMojo.getNetworkNameUnsafe(managedProperties) :
                               '';
  }

  private getBatteryPercentageString_(managedProperties: ManagedProperties|
                                      undefined): string {
    return managedProperties ?
        this.i18n(
            'tetherConnectionBatteryPercentage',
            this.getBatteryPercentageAsString_(managedProperties)) :
        '';
  }

  private getExplanation_(managedProperties: ManagedProperties|
                          undefined): string {
    return managedProperties ?
        loadTimeData.getStringF(
            'tetherConnectionExplanation',
            OncMojo.getNetworkNameUnsafe(managedProperties)) :
        '';
  }

  private getDescriptionTitle_(managedProperties: ManagedProperties|
                               undefined): string {
    return managedProperties ?
        loadTimeData.getStringF(
            'tetherConnectionDescriptionTitle',
            OncMojo.getNetworkNameUnsafe(managedProperties)) :
        '';
  }

  private getBatteryDescription_(managedProperties: ManagedProperties|
                                 undefined): string {
    return managedProperties ?
        this.i18n(
            'tetherConnectionDescriptionBattery',
            this.getBatteryPercentageAsString_(managedProperties)) :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TetherConnectionDialogElement.is]: TetherConnectionDialogElement;
  }
}

customElements.define(
    TetherConnectionDialogElement.is, TetherConnectionDialogElement);
