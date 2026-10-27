// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCollapseRadioButtonElement} from './collapse_radio_button.js';

export function getHtml(this: SettingsCollapseRadioButtonElement) {
  return html`<!--_html_template_start_-->
    <div id="radioCollapse">
      <div aria-checked="${this.getAriaChecked()}"
          aria-disabled="${this.getAriaDisabled()}"
          aria-labelledby="label"
          aria-describedby="sub-label"
          class="disc-wrapper"
          id="button"
          role="radio"
          tabindex="${this.getButtonTabIndex()}"
          @focus="${this.onRadioFocus_}"
          @keydown="${this.onInputKeydown}">
        <div class="disc-border"></div>
        <div class="disc"></div>
      </div>
      <div id="borderWrapper">
        <cr-icon id="buttonIcon" icon="${this.icon}" ?hidden="${!this.icon}"></cr-icon>
        <div id="labelWrapper" class="cr-padded-text">
          <div id="label" aria-hidden="true">
            ${this.label}
            <slot name="label"></slot>
          </div>
          <div ?hidden="${!this.subLabel}" id="sub-label" class="secondary">
            ${this.subLabel}
            <slot name="sub-label"></slot>
          </div>
        </div>
        ${this.pref ? html`
          <cr-policy-pref-indicator .pref="${this.pref}"
              icon-aria-label="${this.indicatorAriaLabel}"
              .associatedValue="${this.name}" @focus="${this.onNonRadioFocus_}">
          </cr-policy-pref-indicator>
        ` : ''}
        <div ?hidden="${this.noCollapse}" class="separator"></div>
        <cr-expand-button id="expandButton" no-hover
              aria-label="${this.expandAriaLabel}"
              ?hidden="${this.noCollapse}" ?expanded="${this.expanded}"
              @expanded-changed="${this.onExpandedChanged_}"
              @click="${this.onExpandClick_}"
              @focus="${this.onNonRadioFocus_}">
        </cr-expand-button>
      </div>
    </div>

    <cr-collapse ?opened="${this.expanded}">
      <slot name="collapse"></slot>
      <slot name="noSelectionCollapse"></slot>
    </cr-collapse>
<!--_html_template_end_-->`;
}
