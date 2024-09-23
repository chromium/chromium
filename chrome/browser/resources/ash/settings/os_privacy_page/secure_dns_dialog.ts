// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when turning off secure DNS
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {SecureDnsMode} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './secure_dns_dialog.html.js';

export interface SettingsSecureDnsDialogElement {
  $: {
    dialog: CrDialogElement,
    cancelButton: CrButtonElement,
    disableButton: CrButtonElement,
  };
}

const SettingsSecureDnsDialogElementBase = PrefsMixin(PolymerElement);

export class SettingsSecureDnsDialogElement extends
    SettingsSecureDnsDialogElementBase {
  static get is() {
    return 'settings-secure-dns-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  /**
   * Sets the pref's mode to false which will turn off the toggle, and closes
   * the dialog.
   */
  private onDisableClicked_(): void {
    // If the user tries to use their own Secure Custom DNS but enters an
    // invalid DNS configuration, the DNS value will not be saved. So in the
    // scenario where the user switches from Secure Custom with invalid config
    // -> OFF -> Secure Custom with invalid config, the underlying pref value
    // will remain OFF. If the user wants to turn DNS to OFF again, the
    // secure-dns-setting-changed WebUI event does not get fired if the mode is
    // OFF -> OFF, so we have to manually sync the toggle state through a new
    // event. the underlying pref's value remains OFF until the DNS config is
    // valid.
    if (this.getPref('dns_over_https.mode').value === SecureDnsMode.OFF) {
      this.dispatchEvent(
          new CustomEvent('dns-settings-invalid-custom-to-off-mode', {
            bubbles: true,
            composed: true,
          }));
    } else {
      this.setPrefValue('dns_over_https.mode', SecureDnsMode.OFF);
    }

    this.$.dialog.close();
  }

  private onCancelButtonClicked_(): void {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsSecureDnsDialogElement.is]: SettingsSecureDnsDialogElement;
  }
}

customElements.define(
    SettingsSecureDnsDialogElement.is, SettingsSecureDnsDialogElement);
