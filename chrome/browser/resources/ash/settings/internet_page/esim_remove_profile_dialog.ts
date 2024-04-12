// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element to remove eSIM profile
 */

import 'chrome://resources/ash/common/cellular_setup/cellular_setup_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import '../settings_shared.css.js';

import {getESimProfile} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ESimOperationResult, ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router, routes} from '../router.js';

import {getTemplate} from './esim_remove_profile_dialog.html.js';

export interface EsimRemoveProfileDialogElement {
  $: {
    dialog: CrDialogElement,
    cancel: CrButtonElement,
    warningMessage: HTMLElement,
  };
}

const EsimRemoveProfileDialogElementBase = I18nMixin(PolymerElement);

export class EsimRemoveProfileDialogElement extends
    EsimRemoveProfileDialogElementBase {
  static get is() {
    return 'esim-remove-profile-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      networkState: {
        type: Object,
        value: null,
      },

      showCellularDisconnectWarning: {
        type: Boolean,
        value: false,
      },

      esimProfileName_: {
        type: String,
        value: '',
      },
    };
  }

  networkState: OncMojo.NetworkStateProperties|null;
  showCellularDisconnectWarning: boolean;
  private esimProfileName_: string;
  private esimProfileRemote_: ESimProfileRemote|null;

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
      this.fireShowErrorToastEvent_();
      this.$.dialog.close();
      return;
    }
    this.esimProfileName_ = this.networkState.name;
    this.$.cancel.focus();
  }

  private getTitleString_(): string {
    if (!this.esimProfileName_) {
      return '';
    }
    return this.i18n('esimRemoveProfileDialogTitle', this.esimProfileName_);
  }

  private onRemoveProfileClick_(): void {
    this.esimProfileRemote_!.uninstallProfile().then((response) => {
      if (response.result === ESimOperationResult.kFailure) {
        this.fireShowErrorToastEvent_();
      }
    });
    this.$.dialog.close();
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
    Router.getInstance().setCurrentRoute(
        routes.INTERNET_NETWORKS, params, /*isPopState=*/ true);
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private getRemoveBtnA11yLabel_(esimProfileName: string): string {
    return this.i18n('eSimRemoveProfileRemoveA11yLabel', esimProfileName);
  }

  private getCancelBtnA11yLabel_(esimProfileName: string): string {
    return this.i18n('eSimRemoveProfileCancelA11yLabel', esimProfileName);
  }

  private fireShowErrorToastEvent_(): void {
    const showErrorToastEvent = new CustomEvent('show-error-toast', {
      bubbles: true,
      composed: true,
      detail: this.i18n('eSimRemoveProfileDialogError'),
    });
    this.dispatchEvent(showErrorToastEvent);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EsimRemoveProfileDialogElement.is]: EsimRemoveProfileDialogElement;
  }
}

customElements.define(
    EsimRemoveProfileDialogElement.is, EsimRemoveProfileDialogElement);
