// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCheckboxElement} from './settings_checkbox.js';

export function getHtml(this: SettingsCheckboxElement) {
  return html`<!--_html_template_start_-->
<div id="outerRow" ?no-sub-label="${!this.hasSubLabel_()}">
  <cr-checkbox id="checkbox" ?checked="${this.checked}"
      @change="${this.onChange_}"
      ?disabled="${this.controlDisabled()}"
      aria-label="${this.label}">
    <div id="label">${this.label} <slot></slot></div>
    <div id="subLabel" class="cr-secondary-text">
      <div .innerHTML="${this.sanitizeInnerHtml_()}"
          @click="${this.onSubLabelClick_}"></div>
      ${this.subLabel}
    </div>
  </cr-checkbox>
  ${this.pref?.controlledBy ? html`
    <cr-policy-pref-indicator .pref="${this.pref}"
        icon-aria-label="${this.label}">
    </cr-policy-pref-indicator>
  ` : ''}
</div>
<!--_html_template_end_-->`;
}
