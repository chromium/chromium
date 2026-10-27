// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from './settings_toggle_button.js';

export function getHtml(this: SettingsToggleButtonElement) {
  return html`<!--_html_template_start_-->
<div id="outerRow" ?no-sub-label="${!this.subLabel}">
  ${this.icon ? html`
    <span id="icon" aria-hidden="true">
      <cr-icon slot="icon" icon="${this.icon}">
      </cr-icon>
    </span>
  ` : ''}
  <div class="flex" id="labelWrapper" ?hidden="${!this.label}">
    <div class="label" aria-hidden="${!this.ariaShowLabel}">${this.label}</div>
    <div class="cr-secondary-text label" id="sub-label">
      ${this.subLabelIcon ? html`
        <span id="sub-label-icon" aria-hidden="true">
          <cr-icon slot="icon" icon="${this.subLabelIcon}">
          </cr-icon>
        </span>
      ` : ''}
      <span id="sub-label-text" aria-hidden="${!this.ariaShowSublabel}">
        ${this.subLabel}
      </span>
      ${this.learnMoreUrl ? html`
        <a id="learn-more" href="${this.learnMoreUrl}" target="_blank"
            aria-labelledby="${this.getLearnMoreAriaLabelledBy_()}"
            aria-description="$i18n{opensInNewTab}"
            @click="${this.onLearnMoreClick_}">
          $i18n{learnMore}
        </a>
        <span id="learn-more-aria-label" aria-hidden="true" hidden>
          ${this.learnMoreAriaLabel}
        </span>
      ` : ''}
      ${this.subLabelWithLink ? html`
        <div id="sub-label-text-with-link"
            .innerHTML="${this.getSubLabelWithLinkContent_()}"
            @click="${this.onSubLabelTextWithLinkClick_}">
        </div>
      ` : ''}
    </div>
  </div>
  <slot name="more-actions"></slot>
  ${this.hasPrefPolicyIndicator() ? html`
    <cr-policy-pref-indicator .pref="${this.pref}"
        icon-aria-label="${this.label}">
    </cr-policy-pref-indicator>
  ` : ''}
  <cr-toggle id="control" ?checked="${this.checked}"
      @change="${this.onChange_}"
      aria-label="${this.getAriaLabel_()}"
      aria-describedby="sub-label-text"
      ?disabled="${this.controlDisabled()}">
  </cr-toggle>
  <slot name="more-actions-after"></slot>
</div>
<!--_html_template_end_-->`;
}
