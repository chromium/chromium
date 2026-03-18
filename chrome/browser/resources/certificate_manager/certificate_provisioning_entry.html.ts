// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateProvisioningEntryElement} from './certificate_provisioning_entry.js';

export function getHtml(this: CertificateProvisioningEntryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cert-box">
  <div class="flex" tabindex="0">${this.model.certProfileName}</div>
  <cr-icon-button class="icon-more-vert" id="dots"
      title="$i18n{moreActions}" @click="${this.onDotsClick_}">
  </cr-icon-button>
  <cr-lazy-render-lit id="menu" .template="${() => html`
    <cr-action-menu role-description="$i18n{menu}">
      <button class="dropdown-item" id="details"
          @click="${this.onDetailsClick_}">
        $i18n{certificateProvisioningDetails}
      </button>
    </cr-action-menu>
  `}">
  </cr-lazy-render-lit>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
