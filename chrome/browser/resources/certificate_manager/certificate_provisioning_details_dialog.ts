// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-details-dialog' allows the user to
 * view the details of an in-progress certificate provisioning process.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
import {CertificateProvisioningBrowserProxyImpl} from './certificate_provisioning_browser_proxy.js';
import {getCss} from './certificate_provisioning_details_dialog.css.js';
import {getHtml} from './certificate_provisioning_details_dialog.html.js';

export interface CertificateProvisioningDetailsDialogElement {
  $: {
    dialog: CrDialogElement,
    refresh: HTMLElement,
  };
}

export class CertificateProvisioningDetailsDialogElement extends CrLitElement {
  static get is() {
    return 'certificate-provisioning-details-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      model: {type: Object},
      advancedExpanded_: {type: Boolean},
    };
  }

  accessor model: CertificateProvisioningProcess = {
    processId: '',
    certProfileId: '',
    certProfileName: '',
    isDeviceWide: false,
    publicKey: '',
    stateId: 0,
    status: '',
    timeSinceLastUpdate: '',
    lastUnsuccessfulMessage: '',
  };
  protected accessor advancedExpanded_: boolean = false;

  close() {
    this.$.dialog.close();
  }

  protected onRefreshClick_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessUpdate(this.model.certProfileId);
  }

  protected onResetClick_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessReset(this.model.certProfileId);
  }

  protected shouldHideLastFailedStatus_(): boolean {
    return this.model.lastUnsuccessfulMessage.length === 0;
  }

  protected onAdvancedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.advancedExpanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-details-dialog':
        CertificateProvisioningDetailsDialogElement;
  }
}

customElements.define(
    CertificateProvisioningDetailsDialogElement.is,
    CertificateProvisioningDetailsDialogElement);
