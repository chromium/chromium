// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateListElement} from './certificate_list.js';

export function getHtml(this: CertificateListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div ?hidden="${this.shouldHideEverything_()}">
  <div id="listHeader"
      class="section-title list-title first
          ${this.getListHeaderAdditionalClass_()}"
      ?hidden="${this.hideHeader}">
    ${this.headerText}
    <div class="header-buttons">
      <cr-button ?hidden="${!this.showImport}" id="importCert"
          aria-label="${this.i18n('certificateManagerV2ImportButtonAriaLabel',
              this.headerText)}"
          @click="${this.onImportCertClick_}">
        $i18n{certificateManagerV2ImportButtonLabel}
      </cr-button>
      <cr-button ?hidden="${!this.showImportAndBind}" id="importAndBindCert"
          aria-label="${this.i18n(
              'certificateManagerV2ImportAndBindButtonAriaLabel',
              this.headerText)}"
          @click="${this.onImportAndBindCertClick_}">
        $i18n{certificateManagerV2ImportAndBindButtonLabel}
      </cr-button>
      <cr-button ?hidden="${this.hideExportButton_()}"
          aria-label="${this.i18n('certificateManagerV2ExportButtonAriaLabel',
              this.headerText)}"
          id="exportCerts" @click="${this.onExportCertsClick_}">
        $i18n{certificateManagerV2ExportButtonLabel}
      </cr-button>
    </div>
    <cr-expand-button id="expandButton" ?expanded="${this.expanded_}" no-hover
        aria-label="${this.i18n('certificateManagerV2ListExpandAriaLabel',
            this.headerText)}"
        ?hidden="${this.hideCollapseButton_()}"
        @expanded-changed="${this.onExpandedChanged_}">
    </cr-expand-button>
  </div>

  <cr-collapse id="certs" ?opened="${this.expanded_}" aria-live="polite"
      class="${this.getCertsClass_()}">
    ${this.certificates_.map(item => html`
      <certificate-entry
          .certSource="${this.certSource}"
          .displayName="${item.displayName}"
          .sha256hashHex="${item.sha256hashHex}"
          ?show-edit-icon="${this.certMetadataEditable}"
          ?is-deletable="${item.isDeletable}"
          @delete-result="${this.onDeleteResult_}">
      </certificate-entry>
    `)}
    <div id="noCertsRow" class="cr-row no-certs" ?hidden="${this.hasCerts_}">
      $i18n{certificateManagerV2NoCertificatesRow}
    </div>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
