// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsOmniboxEverywhereSectionElement} from './omnibox_everywhere_section.js';

export function getHtml(this: SettingsOmniboxEverywhereSectionElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{omniboxEverywhereTitle}">
  <settings-toggle-button id="mainToggle"
      class="first"
      pref-key="omnibox_everywhere.enabled"
      label="$i18n{omniboxEverywhereToggleTitle}"
      sub-label="$i18n{omniboxEverywhereToggleSublabel}"
      learn-more-url="$i18n{omniboxEverywhereLearnMoreURL}">
  </settings-toggle-button>

  <cr-collapse id="expandedContent" ?opened="${this.isEnabled_}">
    <div class="cr-row hr shortcut-setting">
      <div class="flex cr-padded-text">
        <div class="shortcut-label" aria-hidden="true">
          $i18n{omniboxEverywhereShortcutTitle}
        </div>
        <div class="secondary">
          $i18n{omniboxEverywhereShortcutSublabel}
          <a href="$i18n{omniboxEverywhereLearnMoreURL}"
              aria-description="$i18n{opensInNewTab}"
              target="_blank">
            $i18n{learnMore}
          </a>
        </div>
      </div>
      <cr-shortcut-input class="shortcut-input"
          id="shortcutInput"
          input-aria-label="$i18n{omniboxEverywhereShortcutTitle}"
          edit-button-aria-label="$i18n{edit}"
          clear-button-aria-label="$i18n{shortcutClear}"
          .shortcut="${this.registeredShortcut_}"
          allow-ctrl-alt-shortcuts
          @input-capture-change="${this.onInputCaptureChange_}"
          @shortcut-updated="${this.onShortcutUpdated_}">
      </cr-shortcut-input>
    </div>

    <settings-toggle-button id="showShortcutsToggle"
        pref-key="omnibox_everywhere.show_shortcuts"
        label="$i18n{omniboxEverywhereShowShortcutsTitle}"
        sub-label="$i18n{omniboxEverywhereShowShortcutsSublabel}">
    </settings-toggle-button>
  </cr-collapse>
</settings-section>
<!--_html_template_end_-->`;
}
