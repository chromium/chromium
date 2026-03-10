// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CertificateEntryElement} from './certificate_entry.js';

export function getHtml(this: CertificateEntryElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-row cert-row">
  ${this.displayName}
  <cr-input id="certhash" class="input-field cert-hash"
      value="${this.sha256hashHex}" readonly>
    <cr-icon-button id="copy" class="icon-copy-content"
        slot="inline-suffix"
        aria-label="${this.i18n('certificateManagerV2CertHashCopyAriaLabel',
            this.displayName)}"
        @click="${this.onCopyClick_}">
    </cr-icon-button>
  </cr-input>
  <cr-icon-button id="delete" class="icon-picture-delete"
      ?hidden="${!this.isDeletable}"
      aria-label="${this.i18n('certificateManagerV2CertEntryDeleteAriaLabel',
          this.displayName)}"
      @click="${this.onDeleteClick_}">
  </cr-icon-button>
  <cr-icon-button id="view" class="${this.certDetailsIconClass_()}"
      aria-label="${this.i18n('certificateManagerV2CertEntryViewAriaLabel',
          this.displayName)}"
      @click="${this.onViewClick_}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
