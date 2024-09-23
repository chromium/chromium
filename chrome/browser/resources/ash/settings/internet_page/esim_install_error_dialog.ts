// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element handling errors when installing an eSIM
 * profile, such as requiring a confirmation code.
 */

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ESimOperationResult, ESimProfileRemote, ProfileInstallResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './esim_install_error_dialog.html.js';

export interface EsimInstallErrorDialogElement {
  $: {
    installErrorDialog: CrDialogElement,
  };
}

const EsimInstallErrorDialogElementBase = I18nMixin(PolymerElement);

export class EsimInstallErrorDialogElement extends
    EsimInstallErrorDialogElementBase {
  static get is() {
    return 'esim-install-error-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The error code returned when profile install attempt was made in
       * networks list.
       */
      errorCode: {
        type: Object,
        value: null,
      },

      profile: {
        type: Object,
        value: null,
      },

      confirmationCode_: {
        type: String,
        value: '',
        observer: 'onConfirmationCodeChanged_',
      },

      isConfirmationCodeInvalid_: {
        type: Boolean,
        value: false,
      },

      isInstallInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  errorCode: ProfileInstallResult|null;
  profile: ESimProfileRemote|null;
  private confirmationCode_: string;
  private isConfirmationCodeInvalid_: boolean;
  private isInstallInProgress_: boolean;

  private onConfirmationCodeChanged_(): void {
    this.isConfirmationCodeInvalid_ = false;
  }

  private onDoneClicked_(): void {
    if (!this.isConfirmationCodeError_()) {
      this.$.installErrorDialog.close();
      return;
    }
    this.isInstallInProgress_ = true;
    this.isConfirmationCodeInvalid_ = false;

    this.profile!.installProfile(this.confirmationCode_).then((response) => {
      this.isInstallInProgress_ = false;
      if (response.result === ESimOperationResult.kSuccess) {
        this.$.installErrorDialog.close();
        return;
      }
      // TODO(crbug.com/40134918) Only display confirmation code entry if the
      // error was an invalid confirmation code, else display generic error.
      this.isConfirmationCodeInvalid_ = true;
    });
  }

  private onCancelClicked_(): void {
    this.$.installErrorDialog.close();
  }

  private isConfirmationCodeError_(): boolean {
    return this.errorCode === ProfileInstallResult.kErrorNeedsConfirmationCode;
  }

  private isDoneButtonDisabled_(): boolean {
    return this.isConfirmationCodeError_() &&
        (!this.confirmationCode_ || this.isInstallInProgress_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EsimInstallErrorDialogElement.is]: EsimInstallErrorDialogElement;
  }
}

customElements.define(
    EsimInstallErrorDialogElement.is, EsimInstallErrorDialogElement);
