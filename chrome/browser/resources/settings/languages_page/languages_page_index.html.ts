// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsLanguagesPageIndexElement} from './languages_page_index.js';

export function getHtml(this: SettingsLanguagesPageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-languages-page slot="view" id="languages"
      route-path="${this.routes_.LANGUAGES.path}">
  </settings-languages-page>

  <settings-spell-check-page slot="view" id="spellCheck"
      route-path="${this.routes_.SPELL_CHECK.path}">
  </settings-spell-check-page>

  <settings-translate-page slot="view" id="translate">
  </settings-translate-page>

<if expr="not is_macosx">
  <settings-edit-dictionary-page slot="view" id="editDictionary"
      data-parent-view-id="spellCheck"
      route-path="${this.routes_.EDIT_DICTIONARY.path}">
  </settings-edit-dictionary-page>
</if>
</cr-view-manager>
<!--_html_template_end_-->`;
}
