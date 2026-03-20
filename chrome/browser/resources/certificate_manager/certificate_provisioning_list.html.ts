// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateProvisioningListElement} from './certificate_provisioning_list.js';

export function getHtml(this: CertificateProvisioningListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.showProvisioningDetailsDialog_ ? html`
  <certificate-provisioning-details-dialog
      .model="${this.provisioningDetailsDialogModel_}"
      @close="${this.onDialogClose_}">
  </certificate-provisioning-details-dialog>
` : ''}
<div class="header-box" role="heading"
    aria-labelledby="headingLabel"
    ?hidden="${!this.hasCertificateProvisioningEntries_()}">
  <span id="headingLabel" class="flex">
    $i18n{certificateProvisioningListHeader}
  </span>
</div>
${this.provisioningProcesses_.map(item => html`
  <certificate-provisioning-entry .model="${item}">
  </certificate-provisioning-entry>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
