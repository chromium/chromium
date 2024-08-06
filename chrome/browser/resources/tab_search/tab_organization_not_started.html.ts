// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationNotStartedElement} from './tab_organization_not_started.js';

export function getHtml(this: TabOrganizationNotStartedElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="tab-organization-container">
  <tab-organization-not-started-image></tab-organization-not-started-image>
  <div class="tab-organization-text-container">
    <div id="header"
        class="tab-organization-header"
        aria-live="polite"
        aria-relevant="all">
      ${this.getTitle()}
    </div>
    <div class="tab-organization-body">
      ${this.getBody_()}
      ${this.showFre ? html`
<table class="bullet-list">
          <tr>
            <td>
              <cr-icon icon="tab-search:plant" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet1}</td>
          </tr>
          <tr>
            <td>
              <cr-icon icon="tab-search:google" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet2}</td>
          </tr>
          <tr>
            <td>
              <cr-icon icon="tab-search:frame" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet3}</td>
          </tr>
        </table>
        <a class="tab-organization-link"
            role="link"
            tabindex="0"
            @click="${this.onLearnMoreClick_}">
          $i18n{learnMore}
        </a>
      ` : ''}
    </div>
  </div>
  <cr-button class="action-button"
      aria-label="${this.getActionButtonAriaLabel_()}"
      @click="${this.onButtonClick_}">
    ${this.getActionButtonText_()}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
