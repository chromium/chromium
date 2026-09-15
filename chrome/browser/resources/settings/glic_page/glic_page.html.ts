// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGlicPageElement} from './glic_page.js';

export function getHtml(this: SettingsGlicPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{glicSectionTitle}">
  <div>
    <!-- TODO(crbug.com/393445109) Placeholder for final icons. -->
    <cr-link-row id="glicLinkRow"
        start-icon="${this.startIcon_}"
        label="$i18n{glicRowLabel}"
        @click="${this.onGlicPageClick_}">

      <div slot="sub-label">
        $i18n{glicRowSublabel}
        <a id="learnMoreLabel" class="learn-more-label"
            href="$i18n{glicSettingsPageLearnMoreUrl}"
            aria-description="$i18n{opensInNewTab}"
            @click="${this.onSettingsPageLearnMoreClick_}" target="_blank">
          $i18n{learnMore}
        </a>
      </div>
    </cr-link-row>
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
