// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {KeyboardShortcutPageElement} from './keyboard_shortcut_page.js';

export function getHtml(this: KeyboardShortcutPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{searchKeyboardKeyTitle}">
  <div class="cr-row first">
    $i18n{searchKeyboardKeyDescription}
    <settings-dropdown-menu
        id="dropdown"
        label="$i18n{searchKeyboardKeyDescription}"
        pref-key="omnibox.keyword_space_triggering_enabled"
        .menuOptions="${this.keyboardShortcutMenuOptions_}"
        @settings-control-change="${this.onKeyboardShortcutSettingsControlChange_}">
    </settings-dropdown-menu>
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
