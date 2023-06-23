// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './passpoint_remove_dialog.html.js';

const PasspointRemoveDialogElementBase = I18nMixin(PolymerElement);

export class PasspointRemoveDialogElement extends
    PasspointRemoveDialogElementBase {
  static get is() {
    return 'passpoint-remove-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private isPasspointEnabled_: boolean;
  private isPasspointSettingsEnabled_: boolean;

  constructor() {
    super();

    this.isPasspointEnabled_ = loadTimeData.valueExists('isPasspointEnabled') &&
        loadTimeData.getBoolean('isPasspointEnabled');
    this.isPasspointSettingsEnabled_ =
        loadTimeData.valueExists('isPasspointSettingsEnabled') &&
        loadTimeData.getBoolean('isPasspointSettingsEnabled');
  }

  open(): void {
    const dialog = this.getDialog_();
    if (!dialog.open) {
      dialog.showModal();
    }

    this.shadowRoot!.querySelector<CrButtonElement>('#confirmButton')!.focus();
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

  private onCancelClick_(): void {
    this.getDialog_().cancel();
  }

  private onConfirmClick_(): void {
    const event = new CustomEvent('confirm', {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }

  private getDialogTitle_(): string {
    if (this.isPasspointSettingsEnabled_) {
      return this.i18n('networkSectionPasspointGoToSubscriptionTitle');
    }
    return this.i18n('networkSectionPasspointRemovalTitle');
  }

  private hasDescription_(): boolean {
    return this.isPasspointEnabled_ && !this.isPasspointSettingsEnabled_;
  }

  private getDialogInformation_(): string {
    if (this.isPasspointSettingsEnabled_) {
      return this.i18n('networkSectionPasspointGoToSubscriptionInformation');
    }
    return this.i18n('networkSectionPasspointRemovalInformation');
  }

  private getConfirmButtonLabel_(): string {
    if (this.isPasspointSettingsEnabled_) {
      return this.i18n('networkSectionPasspointGoToSubscriptionButtonLabel');
    }
    return this.i18n('confirm');
  }

  private getConfirmButtonA11yLabel_(): string {
    if (this.isPasspointSettingsEnabled_) {
      return this.i18n('passpointRemoveGoToSubscriptionButtonA11yLabel');
    }
    return this.i18n('passpointRemoveConfirmA11yLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PasspointRemoveDialogElement.is]: PasspointRemoveDialogElement;
  }
}

customElements.define(
    PasspointRemoveDialogElement.is, PasspointRemoveDialogElement);
