// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LhsBrandmarkElement} from './lhs_brandmark.js';
import {getHtml as getGoogleWordmarkHtml} from './lhs_google_wordmark.html.js';

export function getHtml(this: LhsBrandmarkElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <!-- Search: aim_header_brandmark.html .lhs-brandmark-superg /
       #DesktopSuperGLogo (tooltip matches #GoogleSearchTooltip).
       NTP: uses cr-icon-button to match the folded rail's #logoButton
       (see cr_components/composebox/threads_rail.html.ts). -->
  <cr-icon-button id="logoButton" suppress-rtl-flip ?inert="${this.expanded}"
      title="${this.i18n('aimThreadsGoogleSearchLabel')}"
      @click="${this.onLogoClick_}">
  </cr-icon-button>
  <!-- Search: aim_header_brandmark.html .lhs-brandmark-wordmark -->
  <a id="wordmark" href="${this.getGoogleBaseUrl_() || nothing}"
      ?inert="${!this.expanded}"
      aria-label="${this.i18n('aimThreadsGoogleSearchLabel')}"
      @click="${this.onLogoClick_}">
    ${getGoogleWordmarkHtml.bind(this)()}
  </a>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
