// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-gmail-otp-disclaimer-dialog' is the dialog that is
 * shown when turning on Gmail OTP filling requires smart features in Gmail to
 * be enabled.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './gmail_otp_disclaimer_dialog.html.js';

export interface SettingsGmailOtpDisclaimerDialogElement {
  $: {
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class SettingsGmailOtpDisclaimerDialogElement extends PolymerElement {
  static get is() {
    return 'settings-gmail-otp-disclaimer-dialog';
  }

  static get template() {
    return getTemplate();
  }

  close() {
    this.$.dialog.close();
  }

  private onConfirmButtonClick_() {
    this.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-gmail-otp-disclaimer-dialog':
        SettingsGmailOtpDisclaimerDialogElement;
  }
}

customElements.define(
    SettingsGmailOtpDisclaimerDialogElement.is,
    SettingsGmailOtpDisclaimerDialogElement);
