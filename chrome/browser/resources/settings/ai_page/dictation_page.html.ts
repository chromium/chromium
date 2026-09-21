// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsDictationPageElement} from './dictation_page.js';

export function getHtml(this: SettingsDictationPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{dictationSettingLabel}"
    route-path="${this.routePath}">
  <div class="section">
    <h2 class="cr-title-text">$i18n{dictationPreferencesHeader}</h2>
    <div class="cr-row keyboard-shortcut-setting">
      <div class="flex cr-padded-text">
        <div class="shortcut-label" aria-hidden>
          $i18n{dictationShortcutLabel}
        </div>
        <div class="secondary">
          $i18n{dictationShortcutSublabel}
          <a id="dictationLearnMoreLabel" class="learn-more-label"
              href="https://support.google.com/chrome?p=voice_typing"
              aria-description="$i18n{opensInNewTab}" target="_blank">
            $i18n{learnMore}
          </a>
        </div>
      </div>
      <cr-shortcut-input id="shortcutInput"
          class="cr-padded-text shortcut-input"
          input-aria-label="$i18n{dictationShortcutLabel}"
          edit-button-aria-label="$i18n{dictationShortcutEditLabel}"
          clear-button-aria-label="$i18n{dictationShortcutClearLabel}"
          .shortcut="${this.registeredShortcut_}"
          allow-ctrl-alt-shortcuts
          @shortcut-updated="${this.onShortcutUpdated_}">
      </cr-shortcut-input>
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
}
