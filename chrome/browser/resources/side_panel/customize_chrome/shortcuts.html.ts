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
    <cr-radio-group id="radioSelection" ?disabled="${!this.show_}"
        .selected="${this.radioSelection_}"
         @selected-changed="${this.onRadioSelectionChanged_}" nested-selectable>
      <div class="option" id="customLinksContainer"
          @click="${this.onCustomLinksClick_}">
        <customize-chrome-button-label label="$i18n{myShortcuts}"
            label-description="$i18n{shortcutsCurated}">
        </customize-chrome-button-label>
        <cr-radio-button id="customLinksButton"
            name="customLinksOption"
            label="$i18n{myShortcuts}"
            hide-label-text>
          <!-- cr-radio-button's aria description references slotted content -->
          <span class="button-aria-describedby">$i18n{shortcutsCurated}</span>
        </cr-radio-button>
      </div>
      <div class="option" id="mostVisitedContainer"
          @click="${this.onMostVisitedClick_}">
        <customize-chrome-button-label label="$i18n{mostVisited}"
            label-description="$i18n{shortcutsSuggested}">
        </customize-chrome-button-label>
        <cr-radio-button id="mostVisitedButton"
            name="mostVisitedOption"
            label="$i18n{mostVisited}"
            hide-label-text>
          <!-- cr-radio-button's aria description references slotted content -->
          <span class="button-aria-describedby">$i18n{shortcutsSuggested}</span>
        </cr-radio-button>
      </div>
    </cr-radio-group>
  </cr-collapse>
</div>
<!--_html_template_end_-->`;
}
