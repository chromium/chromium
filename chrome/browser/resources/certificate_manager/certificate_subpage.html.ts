// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateSubpageElement} from './certificate_subpage.js';

export function getHtml(this: CertificateSubpageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header" class="cr-row first">
  <cr-icon-button id="backButton" class="icon-arrow-back"
      aria-label="${this.i18n('certificateManagerV2SubpageBackButtonAriaLabel',
                              this.subpageTitle)}"
      aria-roledescription="${this.i18n(
          'certificateManagerV2SubpageBackButtonAriaRoleDescription',
          this.subpageTitle)}"
      @click="${this.onBackButtonClick_}">
  </cr-icon-button>
  <h2 class="cr-title-text">${this.subpageTitle}</h2>
</div>
${this.subpageCertLists.map(item => html`
  <certificate-list
      .certSource="${item.certSource}"
      .headerText="${item.headerText}"
      .certMetadataEditable="${item.certMetadataEditable}"
      ?hide-export="${item.hideExport}"
      ?show-import="${item.showImport}"
      ?show-import-and-bind="${item.showImportAndBind}"
      ?hide-if-empty="${item.hideIfEmpty}"
      ?hide-header="${item.hideHeader}"
      in-subpage>
  </certificate-list>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
