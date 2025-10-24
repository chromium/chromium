// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShortcutsElement} from './shortcuts.js';

export function getHtml(this: ShortcutsElement) {
  return html`<!--_html_template_start_-->
<div id="showToggleContainer" class="sp-card-content"
    @click="${this.onShowToggleClick_}">
  <div id="showTitle">$i18n{showShortcutsToggle}</div>
  <cr-toggle id="showToggle" title="$i18n{showShortcutsToggle}"
      ?checked="${this.show_}" @change="${this.onShowToggleChange_}">
  </cr-toggle>
</div>
<div id="options">
  <cr-collapse ?opened="${this.show_}" ?no-animation="${!this.initialized_}">
    <hr class="sp-hr">
    <div id="enterpriseShortcutsMixedContainer" class="option" @click="${
      this.onShowEnterpriseShortcutsClick_}" ?hidden="${
  !this.showEnterprisePersonalMixedSidepanel_()}">
      ${
      this.getEnterpriseShortcutConfigs_()
          .map(item => html`
          <customize-chrome-button-label label="${item.title}"
              label-description="${item.description}">
          </customize-chrome-button-label>
          <cr-checkbox id="enterpriseToggle" class="label-first"
              ?checked="${this.showEnterpriseShortcuts_}"
              @change="${this.onShowEnterpriseShortcutsChange_}">
          </cr-checkbox>
      `)}
    </div>
    <div id="personalShortcutsContainer" class="option" @click="${
      this.onShowPersonalShortcutsClick_}" ?hidden="${
  !this.showEnterprisePersonalMixedSidepanel_()}">
      <customize-chrome-button-label label="$i18n{showPersonalShortcutsToggle}"
          label-description="$i18n{showPersonalShortcutsToggleDescription}">
      </customize-chrome-button-label>
      <cr-checkbox id="personalToggle" class="label-first"
          ?checked="${this.showPersonalShortcuts_}"
          @change="${this.onShowPersonalShortcutsChange_}">
      </cr-checkbox>
    </div>
    <cr-radio-group id="radioSelection"
        class="${
      this.showEnterprisePersonalMixedSidepanel_() ?
      'sub-options' :
      ''}"
        ?disabled="${this.getRadioSelectionDisabled_()}"
        .selected="${this.radioSelection_}"
         @selected-changed="${this.onRadioSelectionChanged_}" nested-selectable>
      ${
      this.getRadioSelectionShortcutConfigs_()
          .map(
              item => html`
        <div class="option" id="${item.containerName}" @click="${
                  () => this.onOptionClick_(item.type)}">
          <customize-chrome-button-label label="${item.title}"
              label-description="${item.description}">
          </customize-chrome-button-label>
          <cr-radio-button name="${item.buttonName}"
              label="${item.title}"
              hide-label-text>
            <!-- cr-radio-button's aria description references slotted content
             -->
            <span class="button-aria-describedby">${item.description}</span>
          </cr-radio-button>
        </div>
      `)}
    </cr-radio-group>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
}
