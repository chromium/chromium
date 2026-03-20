// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {CertificateSource} from './certificate_manager.mojom-webui.js';
import type {CrsSectionElement} from './crs_section.js';

export function getHtml(this: CrsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <h2 class="page-title">$i18n{certificateManagerV2CRSCerts}</h2>
  $i18n{certificateManagerV2CRSCertsDescription}
  <a href="${this.crsLearnMoreUrl_}" target="_blank"
      aria-label="$i18n{certificateManagerV2CRSLearnMoreLinkAriaLabel}">
    $i18n{certificateManagerV2CRSLearnMoreLink}
  </a>
  <certificate-list
      id="crsCerts"
      no-collapse
      .certSource="${CertificateSource.kChromeRootStore}"
      header-text="$i18n{certificateManagerV2TrustedCertsList}">
  </certificate-list>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
