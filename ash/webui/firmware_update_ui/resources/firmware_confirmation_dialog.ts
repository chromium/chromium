// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import './icons.html.js';
import './firmware_shared.css.js';
import './firmware_shared_fonts.css.js';
import './strings.m.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './firmware_confirmation_dialog.html.js';
import {FirmwareUpdate} from './firmware_update.mojom-webui.js';
import {OpenConfirmationDialogEventDetail, OpenUpdateDialogEventDetail} from './firmware_update_types.js';
import {isTrustedReportsFirmwareEnabled} from './firmware_update_utils.js';

/**
 * @fileoverview
 * 'firmware-confirmation-dialog' provides information about the update and
 *  allows users to either cancel or begin the installation.
 */

const FirmwareConfirmationDialogElementBase =
    I18nMixin(PolymerElement) as {new (): PolymerElement & I18nMixinInterface};

export class FirmwareConfirmationDialogElement extends
    FirmwareConfirmationDialogElementBase {
  static get is() {
    return 'firmware-confirmation-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      update: {
        type: Object,
      },

      open: {
        type: Boolean,
        value: false,
      },

      shouldShowDisclaimer: {
        type: Boolean,
        value: false,
      },
    };
  }

  update: FirmwareUpdate;
  open: boolean = false;
  private shouldShowDisclaimer: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener(
        'open-confirmation-dialog',
        (e) => this.onOpenConfirmationDialog(
            e as CustomEvent<OpenConfirmationDialogEventDetail>));

    this.shouldShowDisclaimer = isTrustedReportsFirmwareEnabled();
  }

  protected openUpdateDialog(): void {
    this.closeDialog();
    this.dispatchEvent(
        new CustomEvent<OpenUpdateDialogEventDetail>('open-update-dialog', {
          bubbles: true,
          composed: true,
          detail: {update: this.update, inflight: false},
        }));
  }

  protected closeDialog(): void {
    this.open = false;
  }

  protected computeTitle(): string {
    return this.i18n(
        'confirmationTitle', mojoString16ToString(this.update.deviceName));
  }

  /** Event callback for 'open-confirmation-dialog'. */
  private onOpenConfirmationDialog(
      event: CustomEvent<OpenConfirmationDialogEventDetail>): void {
    this.update = event.detail.update;
    this.open = true;
  }
}

declare global {
  interface HTMLElementEventMap {
    'open-confirmation-dialog': CustomEvent<OpenConfirmationDialogEventDetail>;
    'open-update-dialog': CustomEvent<OpenUpdateDialogEventDetail>;
  }

  interface HTMLElementTagNameMap {
    [FirmwareConfirmationDialogElement.is]: FirmwareConfirmationDialogElement;
  }
}

customElements.define(
    FirmwareConfirmationDialogElement.is, FirmwareConfirmationDialogElement);
