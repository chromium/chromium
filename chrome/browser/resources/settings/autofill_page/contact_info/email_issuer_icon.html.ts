// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsEmailIssuerIconElement} from './email_issuer_icon.js';

export function getHtml(this: SettingsEmailIssuerIconElement) {
  return html`<!--_html_template_start_-->
<img id="logo" is="cr-auto-img" static-encode clear-src
    auto-src="${this.sanitizedLogoUrl_}" alt="" @load="${this.onLogoLoad_}"
    @error="${this.onLogoError_}" ?hidden="${!this.shouldShowLogo_()}">
<site-favicon url="${this.issuerSite}" ?hidden="${this.shouldShowLogo_()}">
</site-favicon>
<!--_html_template_end_-->`;
}
