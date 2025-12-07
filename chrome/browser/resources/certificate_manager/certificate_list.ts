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
import './certificate_manager_style.css.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './certificate_list.html.js';
import type {ActionResult, CertificateSource, SummaryCertInfo} from './certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from './certificates_browser_proxy.js';

const CertificateListElementBase = I18nMixin(PolymerElement);

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

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      certSource: Number,
      headerText: String,

      showImport: {
        type: Boolean,
        value: false,
      },

      showImportAndBind: {
        type: Boolean,
        value: false,
      },

      // True if the list should not be collapsible.
      // Empty lists will always not be collapsible.
      noCollapse: {
        type: Boolean,
        value: false,
      },

      // True if the export button should be hidden.
      // Export button may also be hidden if there are no certs in the list.
      hideExport: {
        type: Boolean,
        value: false,
      },

      // True if the entire list (including the header) should be hidden if the
      // list is empty.
      hideIfEmpty: {
        type: Boolean,
        value: false,
      },

      // True if the header should be hidden. This will make the list
      // non-collapsible.
      hideHeader: {
        type: Boolean,
        value: false,
      },

      // True if the cert metadata is editable
      certMetadataEditable: {
        type: Boolean,
        value: false,
      },

      inSubpage: {
        type: Boolean,
        value: false,
      },

      expanded_: {
        type: Boolean,
        value: true,
      },

      certificates_: {
        type: Array,
        value: () => [],
      },

      hideEverything_: {
        type: Boolean,
        computed: 'computeHideEverything_(certificates_)',
      },

      hasCerts_: {
        type: Boolean,
        computed: 'computeHasCerts_(certificates_)',
      },
    };
  }

  declare certSource: CertificateSource;
  declare headerText: string;
  declare certMetadataEditable: boolean;
  declare showImport: boolean;
  declare showImportAndBind: boolean;
  declare hideExport: boolean;
  declare hideHeader: boolean;
  declare inSubpage: boolean;
  declare noCollapse: boolean;
  declare hideIfEmpty: boolean;
  declare private expanded_: boolean;
  declare private hideEverything_: boolean;
  declare private certificates_: SummaryCertInfo[];
  declare private hasCerts_: boolean;

  override ready() {
    super.ready();

    this.refreshCertificates();

    if (!this.inSubpage) {
      this.$.certs.classList.add('card');
    }
    if (this.inSubpage) {
      this.$.listHeader.classList.add('subpage-padding');
    }

    const proxy = CertificatesBrowserProxy.getInstance();
    proxy.callbackRouter.triggerReload.addListener(
        this.onRefreshRequested_.bind(this));
  }

  private onRefreshRequested_(certSources: CertificateSource[]) {
    if (certSources.includes(this.certSource)) {
      this.refreshCertificates();
    }
  }

  private refreshCertificates() {
    CertificatesBrowserProxy.getInstance()
        .handler.getCertificates(this.certSource)
        .then((results: {certs: SummaryCertInfo[]}) => {
          this.certificates_ = results.certs;
        });
  }

  private onExportCertsClick_(e: Event) {
    // Export button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance().handler.exportCertificates(
        this.certSource);
  }

  private onImportCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance()
        .handler.importCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  private onImportAndBindCertClick_(e: Event) {
    // Import button click shouldn't collapse the list as well.
    e.stopPropagation();
    CertificatesBrowserProxy.getInstance()
        .handler.importAndBindCertificate(this.certSource)
        .then(this.handleImportResult.bind(this));
  }

  private handleImportResult(value: {result: ActionResult|null}) {
    if (value.result !== null && value.result.success !== undefined) {
      // On successful import, refresh the certificate list.
      this.refreshCertificates();
    }
    this.dispatchEvent(new CustomEvent(
        'import-result',
        {composed: true, bubbles: true, detail: value.result}));
  }

  private onDeleteResult_(e: CustomEvent<ActionResult|null>) {
    const result = e.detail;
    if (result !== null && result.success !== undefined) {
      // On successful deletion, refresh the certificate list.
      this.refreshCertificates();
      this.$.importCert.focus();
    }
  }

  private computeHasCerts_(): boolean {
    return this.certificates_.length > 0;
  }

  private computeHideEverything_(): boolean {
    return this.hideIfEmpty && this.certificates_.length === 0;
  }

  private hideCollapseButton_(): boolean {
    return this.noCollapse || !this.hasCerts_;
  }

  private hideExportButton_(): boolean {
    return this.hideExport || !this.hasCerts_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-list': CertificateListElement;
  }
}

customElements.define(CertificateListElement.is, CertificateListElement);
