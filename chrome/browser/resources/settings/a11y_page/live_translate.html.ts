// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsLiveTranslateElement} from './live_translate.js';

export function getHtml(this: SettingsLiveTranslateElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-row cr-row-with-template">
  <settings-toggle-button id="liveTranslateToggleButton"
      pref-key="accessibility.captions.live_translate_enabled"
      @change="${this.onLiveTranslateEnabledChange_}"
      label="$i18n{captionsEnableLiveTranslateTitle}"
      sub-label="$i18n{captionsEnableLiveTranslateSubtitle}">
  </settings-toggle-button>
</div>
<cr-collapse ?opened="${this.isLiveTranslateEnabled_}">
  <div class="cr-row continuation subsection-group">
    <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsLiveTranslateTargetLanguage}
        <div class="secondary">
          $i18n{captionsLiveTranslateTargetLanguageSubtitle}
        </div>
    </div>
    <settings-dropdown-menu id="targetLanguageDropdown"
        class="language-dropdown"
        pref-key="accessibility.captions.live_translate_target_language"
        .menuOptions="${this.translatableLanguages_}"
        label="$i18n{captionsLiveTranslateTargetLanguage}">
    </settings-dropdown-menu>
  </div>
</cr-collapse>
<!--_html_template_end_-->`;
  // clang-format on
}
