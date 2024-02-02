// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-multidevice-forget-device-dialog' is the dialog used to forget a
 * connected android phone.
 */
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multidevice_forget_device_dialog.html.js';

interface SettingsMultideviceForgetDeviceDialogElement {
  $: {forgetDeviceDialog: CrDialogElement};
}

class SettingsMultideviceForgetDeviceDialogElement extends PolymerElement {
  static get is() {
    return 'settings-multidevice-forget-device-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  private closeDialog_(): void {
    this.$.forgetDeviceDialog.close();
  }

  private onConfirmClick_(): void {
    const forgetDeviceRequestedEvent =
        new CustomEvent('forget-device-requested', {
          bubbles: true,
          composed: true,
        });
    this.dispatchEvent(forgetDeviceRequestedEvent);
    this.closeDialog_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceForgetDeviceDialogElement.is]:
        SettingsMultideviceForgetDeviceDialogElement;
  }
}

customElements.define(
    SettingsMultideviceForgetDeviceDialogElement.is,
    SettingsMultideviceForgetDeviceDialogElement);
