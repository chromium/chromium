// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FeatureShortcutsPageElement} from './feature_shortcuts_page.js';

export function getHtml(this: FeatureShortcutsPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{searchFeatureShortcuts}">
  <!-- Active feature & extension shortcuts -->
  <cr-expand-button id="activeShortcutsRow"
      ?expanded="${this.activeShortcutsExpanded_}"
      @expanded-changed="${this.onActiveShortcutsExpandedChanged_}"
      class="cr-row first">
    <div class="expand-row">
      <div class="text-content-group">
        $i18n{searchFeatureShortcutsActiveShortcuts}
        <div class="secondary">
          $i18n{searchFeatureShortcutsActiveShortcutsExplanation}
        </div>
      </div>
      <div class="separator"></div>
    </div>
  </cr-expand-button>
  <cr-collapse ?opened="${this.activeShortcutsExpanded_}">
    <settings-search-engines-list
        id="activeShortcutsList"
        ?hidden="${!this.activeShortcuts.length}"
        .engines="${this.activeShortcuts}" show-shortcut>
    </settings-search-engines-list>
    <div class="no-shortcuts-found" id="noActiveShortcutsFound"
        ?hidden="${this.activeShortcuts.length > 0}">
      $i18n{searchNoFeatureShortcutsFound}
    </div>
  </cr-collapse>

  <!-- Inactive feature & extension shortcuts -->
  <cr-expand-button id="inactiveShortcutsRow"
      ?expanded="${this.inactiveShortcutsExpanded_}"
      @expanded-changed="${this.onInactiveShortcutsExpandedChanged_}"
      class="cr-row">
    <div class="expand-row">
      <div class="text-content-group">
        $i18n{searchFeatureShortcutsInactiveShortcuts}
        <div class="secondary">
          $i18n{searchFeatureShortcutsInactiveShortcutsExplanation}
        </div>
      </div>
      <div class="separator"></div>
    </div>
  </cr-expand-button>
  <cr-collapse ?opened="${this.inactiveShortcutsExpanded_}">
    <settings-search-engines-list
        id="inactiveShortcutsList"
        ?hidden="${!this.inactiveShortcuts.length}"
        .engines="${this.inactiveShortcuts}" show-shortcut>
    </settings-search-engines-list>
    <div class="no-shortcuts-found" id="noInactiveShortcutsFound"
        ?hidden="${this.inactiveShortcuts.length > 0}">
      $i18n{searchNoFeatureShortcutsFound}
    </div>
  </cr-collapse>
</settings-section>
<!--_html_template_end_-->`;
}
