// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-list' component shows a list of
 * certificates with a header, an expander, and optionally an "export all"
 * button.
 *
 * This component is used in the new Certificate Management UI in
 * ./certificate_manager.ts.
 */


import '/strings.m.js';
import './certificate_entry.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './certificate_list.css.js';
import {getHtml} from './certificate_list.html.js';
import {CertificateSource} from './certificate_manager.mojom-webui.js';
import type {ActionResult, SummaryCertInfo} from './certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from './certificates_browser_proxy.js';

const CertificateListElementBase = I18nMixinLit(CrLitElement);

export interface CertificateListElement {
  $: {
    certs: CrCollapseElement,
    exportCerts: HTMLElement,
    importCert: HTMLElement,
    importAndBindCert: HTMLElement,
    noCertsRow: HTMLElement,
    listHeader: HTMLElement,
    expandButton: HTMLElement,
  };
}

export class CertificateListElement extends CertificateListElementBase {
  static get is() {
    return 'certificate-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      certSource: {type: Number},
      headerText: {type: String},
      showImport: {type: Boolean},
      showImportAndBind: {type: Boolean},
      noCollapse: {type: Boolean},
      hideExport: {type: Boolean},
      hideIfEmpty: {type: Boolean},
      hideHeader: {type: Boolean},
      certMetadataEditable: {type: Boolean},
      inSubpage: {type: Boolean},
      expanded_: {type: Boolean},
      certificates_: {type: Array},
      hasCerts_: {type: Boolean},
    };
  }

  accessor certSource: CertificateSource = CertificateSource.MIN_VALUE;
  accessor headerText: string = '';
  accessor showImport: boolean = false;
  accessor showImportAndBind: boolean = false;
  accessor noCollapse: boolean = false;
  accessor hideExport: boolean = false;
  accessor hideIfEmpty: boolean = false;
  accessor hideHeader: boolean = false;
  accessor certMetadataEditable: boolean = false;
  accessor inSubpage: boolean = false;
  protected accessor expanded_: boolean = true;
  protected accessor certificates_: SummaryCertInfo[] = [];
  protected accessor hasCerts_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('certificates_')) {
      this.hasCerts_ = this.certificates_.length > 0;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.refreshCertificates_();

    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.callbackRouter.triggerReload.addListener(
        this.onRefreshRequested_.bind(this));
  }

  private onRefreshRequested_(certSources: CertificateSource[]) {
    if (certSources.includes(this.certSource)) {
      this.refreshCertificates_();
    }
  }

  protected refreshCertificates_() {
    CertificatesBrowserProxy.getInstance()
        .handler.getCertificates(this.certSource)
        .then((results: {certs: SummaryCertInfo[]}) => {
          this.certificates_ = results.certs;
        });
  }

  protected onExportCertsClick_(e: Event) {
    // Export button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance().handler.exportCertificates(
        this.certSource);
  }

  protected onImportCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance()
        .handler.importCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  protected onImportAndBindCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance()
        .handler.importAndBindCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  private handleImportResult(value: {result: ActionResult|null}) {
    if (value.result !== null && value.result.success !== undefined) {
      // On successful import, refresh the certificate list.
      this.refreshCertificates_();
    }
    this.fire('import-result', value.result);
  }

  protected onDeleteResult_(e: CustomEvent<ActionResult|null>) {
    const result = e.detail;
    if (result !== null && result.success !== undefined) {
      // On successful deletion, refresh the certificate list.
      this.refreshCertificates_();
      this.$.importCert.focus();
    }
  }

  protected shouldHideEverything_(): boolean {
    return this.hideIfEmpty && this.certificates_.length === 0;
  }

  protected hideCollapseButton_(): boolean {
    return this.noCollapse || !this.hasCerts_;
  }

  protected hideExportButton_(): boolean {
    return this.hideExport || !this.hasCerts_;
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected getListHeaderAdditionalClass_(): string {
    return this.inSubpage ? 'subpage-padding' : '';
  }

  protected getCertsClass_(): string {
    return this.inSubpage ? '' : 'card';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-list': CertificateListElement;
  }
}

customElements.define(CertificateListElement.is, CertificateListElement);
