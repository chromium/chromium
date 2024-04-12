// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to rename eSIM profile name
 */

import 'chrome://resources/ash/common/cellular_setup/cellular_setup_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {getESimProfile} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {ESimOperationResult, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './esim_rename_dialog.html.js';

const MAX_INPUT_LENGTH = 20;

const MIN_INPUT_LENGTH = 1;

const EMOJI_REGEX_EXP =
    /(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])/gi;

export interface EsimRenameDialogElement {
  $: {
    profileRenameDialog: CrDialogElement,
    warningMessage: HTMLElement,
  };
}

const EsimRenameDialogElementBase = I18nMixin(PolymerElement);

export class EsimRenameDialogElement extends EsimRenameDialogElementBase {
  static get is() {
    return 'esim-rename-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Used to reference the MAX_INPUT_LENGTH constant in HTML. */
      maxInputLength: {
        type: Number,
        value: MAX_INPUT_LENGTH,
        readonly: true,
      },

      networkState: {
        type: Object,
        value: null,
      },

      showCellularDisconnectWarning: {
        type: Boolean,
        value: false,
      },

      errorMessage_: {
        type: String,
        value: '',
      },

      esimProfileName_: {
        type: String,
        value: '',
        observer: 'onEsimProfileNameChanged_',
      },

      isInputInvalid_: {
        type: Boolean,
        value: false,
      },

      isRenameInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  maxInputLength: number;
  networkState: OncMojo.NetworkStateProperties|null;
  showCellularDisconnectWarning: boolean;
  private errorMessage_: string;
  private esimProfileName_: string;
  private esimProfileRemote_: ESimProfileRemote|null;
  private isInputInvalid_: boolean;
  private isRenameInProgress_: boolean;

  constructor() {
    super();

    this.esimProfileRemote_ = null;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.init_();
  }

  private async init_(): Promise<void> {
    if (!(this.networkState &&
          this.networkState.type === NetworkType.kCellular)) {
      return;
    }
    this.esimProfileRemote_ =
        await getESimProfile(this.networkState.typeState.cellular!.iccid);
    // Fail gracefully if init is incomplete, see crbug/1194729.
    if (!this.esimProfileRemote_) {
      this.errorMessage_ = this.i18n('eSimRenameProfileDialogError');
    }
    this.esimProfileName_ = this.networkState.name;

    if (!this.errorMessage_) {
      this.shadowRoot!.querySelector<CrInputElement>(
                          '#eSimprofileName')!.focus();
    }
  }

  /**
   * @param {Event} event
   * @private
   */
  private async onRenameDialogDoneClick_(): Promise<void> {
    if (this.errorMessage_) {
      this.$.profileRenameDialog.close();
      return;
    }

    this.isRenameInProgress_ = true;

    // The C++ layer uses std::u16string, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted
    // to an array of 16 bit character codes that match std::u16string.
    const name = stringToMojoString16(this.esimProfileName_);

    const response = await this.esimProfileRemote_!.setProfileNickname(name);
    this.handleSetProfileNicknameResponse_(response.result);
  }

  private handleSetProfileNicknameResponse_(result: ESimOperationResult): void {
    this.isRenameInProgress_ = false;
    if (result === ESimOperationResult.kFailure) {
      const showErrorToastEvent = new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('eSimRenameProfileDialogError'),
      });
      this.dispatchEvent(showErrorToastEvent);
    }
    this.$.profileRenameDialog.close();
  }

  private onCancelClick_(): void {
    this.$.profileRenameDialog.close();
  }

  /**
   * Observer for esimProfileName_ that sanitizes its value by removing any
   * Emojis and truncating it to MAX_INPUT_LENGTH. This method will be
   * recursively called until esimProfileName_ is fully sanitized.
   */
  private onEsimProfileNameChanged_(_newValue: string, oldValue: string): void {
    if (oldValue) {
      const sanitizedOldValue = oldValue.replace(EMOJI_REGEX_EXP, '');
      // If sanitizedOldValue.length > MAX_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isInputInvalid_ = sanitizedOldValue.length > MAX_INPUT_LENGTH;
    } else {
      this.isInputInvalid_ = false;
    }

    // Remove all Emojis from the name.
    const sanitizedProfileName =
        this.esimProfileName_.replace(EMOJI_REGEX_EXP, '');

    // Truncate the name to MAX_INPUT_LENGTH.
    this.esimProfileName_ = sanitizedProfileName.substring(0, MAX_INPUT_LENGTH);
  }

  private getInputInfoClass_(isInputInvalid: boolean): string {
    return isInputInvalid ? 'error' : '';
  }

  /**
   * Returns a formatted string containing the current number of characters
   * entered in the input compared to the maximum number of characters allowed.
   */
  private getInputCountString_(esimProfileName: string): string {
    // minimumIntegerDigits is 2 because we want to show a leading zero if
    // length is less than 10.
    return this.i18n(
        'eSimRenameProfileInputCharacterCount',
        esimProfileName.length.toLocaleString(
            /*locales=*/ undefined, {minimumIntegerDigits: 2}),
        MAX_INPUT_LENGTH.toLocaleString());
  }

  private isDoneButtonDisabled_(
      isRenameInProgress: boolean, esimProfileName: string): boolean {
    if (isRenameInProgress) {
      return true;
    }
    return esimProfileName.length < MIN_INPUT_LENGTH;
  }

  private getDoneBtnA11yLabel_(esimProfileName: string): string {
    return this.i18n('eSimRenameProfileDoneBtnA11yLabel', esimProfileName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EsimRenameDialogElement.is]: EsimRenameDialogElement;
  }
}

customElements.define(EsimRenameDialogElement.is, EsimRenameDialogElement);
