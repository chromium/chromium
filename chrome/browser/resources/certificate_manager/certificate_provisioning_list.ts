// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-list' is an element that displays a
 * list of certificate provisioning processes.
 */
import './certificate_provisioning_details_dialog.js';
import './certificate_provisioning_entry.js';

import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import type {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';
import {CertificateProvisioningBrowserProxyImpl} from './certificate_provisioning_browser_proxy.js';
import {getCss} from './certificate_provisioning_list.css.js';
import {getHtml} from './certificate_provisioning_list.html.js';

const CertificateProvisioningListElementBase =
    WebUiListenerMixinLit(CrLitElement);

export class CertificateProvisioningListElement extends
    CertificateProvisioningListElementBase {
  static get is() {
    return 'certificate-provisioning-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      removeHeaderPadding: {
        type: Boolean,
        reflect: true,
      },
      provisioningProcesses_: {type: Array},

      /**
       * The model to be passed to certificate provisioning details dialog.
       */
      provisioningDetailsDialogModel_: {type: Object},

      showProvisioningDetailsDialog_: {type: Boolean},
    };
  }

  accessor removeHeaderPadding: boolean = false;
  protected accessor provisioningProcesses_: CertificateProvisioningProcess[] =
      [];
  protected accessor provisioningDetailsDialogModel_:
      CertificateProvisioningProcess|null = null;
  protected accessor showProvisioningDetailsDialog_: boolean = false;
  private previousAnchor_: HTMLElement|null = null;

  /**
   * @return Whether |provisioningProcesses_| contains at least one entry.
   */
  protected hasCertificateProvisioningEntries_(): boolean {
    return this.provisioningProcesses_.length !== 0;
  }

  /**
   * @param certProvisioningProcesses The currently active certificate
   *     provisioning processes
   */
  private onCertificateProvisioningProcessesChanged_(
      certProvisioningProcesses: CertificateProvisioningProcess[]) {
    this.provisioningProcesses_ = certProvisioningProcesses;

    // If a cert provisioning process details dialog is being shown, update its
    // model.
    if (!this.provisioningDetailsDialogModel_) {
      return;
    }

    const certProfileId = this.provisioningDetailsDialogModel_.certProfileId;
    const newDialogModel = this.provisioningProcesses_.find((process) => {
      return process.certProfileId === certProfileId;
    });
    if (newDialogModel) {
      this.provisioningDetailsDialogModel_ = newDialogModel;
    } else {
      // Close cert provisioning process details dialog if the process is no
      // longer in the list eg. when process completed successfully.
      this.shadowRoot.querySelector(
                         'certificate-provisioning-details-dialog')!.close();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'certificate-provisioning-processes-changed',
        (certProvisioningProcesses: CertificateProvisioningProcess[]) =>
            this.onCertificateProvisioningProcessesChanged_(
                certProvisioningProcesses));
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .refreshCertificateProvisioningProcesses();
  }

  override firstUpdated() {
    this.addEventListener(
        CertificateProvisioningViewDetailsActionEvent, event => {
          const detail = event.detail;
          this.provisioningDetailsDialogModel_ = detail.model;
          this.previousAnchor_ = detail.anchor;
          this.showProvisioningDetailsDialog_ = true;
          event.stopPropagation();
          CertificateProvisioningBrowserProxyImpl.getInstance()
              .refreshCertificateProvisioningProcesses();
        });
  }

  protected onDialogClose_() {
    this.showProvisioningDetailsDialog_ = false;
    focusWithoutInk(this.previousAnchor_!);
    this.previousAnchor_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-list': CertificateProvisioningListElement;
  }
}

customElements.define(
    CertificateProvisioningListElement.is, CertificateProvisioningListElement);
