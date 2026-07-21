// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsCaptionsPageElement} from './captions_page.js';

export function getHtml(this: SettingsCaptionsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{captionsTitle}"
    route-path="${this.routePath}">
  ${this.enableLiveCaption_ ? html`
    <settings-live-caption></settings-live-caption>
  ` : ''}
  <div class="cr-row">
    <h2 class="start">$i18n{captionsPreferencesTitle}</h2>
  </div>
  <div class="cr-row first cr-row-no-top-gap">
    <div class="start">$i18n{captionsPreferencesSubtitle}</div>
  </div>
  <div class="preview-box">
    <span style="
        font-size:${this.textSizePref_?.value};
        font-family:${this.getFontFamily_()};
        background-color: ${this.computeBackgroundColor_()};
        color: ${this.computeTextColor_()};
        text-shadow: ${this.textShadowPref_?.value};
        padding: ${this.computePadding_(this.textSizePref_?.value || '')}">
      $i18n{quickBrownFox}
    </span>
  </div>
  <div class="list-frame">
    <div class="list-item underbar first">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsTextSize}
      </div>
      <settings-dropdown-menu id="captionsTextSize"
          label="$i18n{captionsTextSize}"
          pref-key="accessibility.captions.text_size"
          .menuOptions="${this.textSizeOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item underbar">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsTextFont}
      </div>
      <settings-dropdown-menu id="captionsTextFont"
          label="$i18n{captionsTextFont}"
          pref-key="accessibility.captions.text_font"
          .menuOptions="${this.textFontOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item underbar">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsTextColor}
      </div>
      <settings-dropdown-menu id="captionsTextColor"
          label="$i18n{captionsTextColor}"
          pref-key="accessibility.captions.text_color"
          .menuOptions="${this.colorOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item underbar">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsTextOpacity}
      </div>
      <settings-dropdown-menu id="captionsTextOpacity"
          label="$i18n{captionsTextOpacity}"
          pref-key="accessibility.captions.text_opacity"
          .menuOptions="${this.textOpacityOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item underbar">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsTextShadow}
      </div>
      <settings-dropdown-menu id="captionsTextShadow"
          label="$i18n{captionsTextShadow}"
          pref-key="accessibility.captions.text_shadow"
          .menuOptions="${this.textShadowOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item underbar">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsBackgroundColor}
      </div>
      <settings-dropdown-menu id="captionsBackgroundColor"
          label="$i18n{captionsBackgroundColor}"
          pref-key="accessibility.captions.background_color"
          .menuOptions="${this.colorOptions_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-item">
      <div class="start cr-padded-text" aria-hidden="true">
        $i18n{captionsBackgroundOpacity}
      </div>
      <settings-dropdown-menu id="captionsBackgroundOpacity"
          label="$i18n{captionsBackgroundOpacity}"
          pref-key="accessibility.captions.background_opacity"
          .menuOptions="${this.backgroundOpacityOptions_}">
      </settings-dropdown-menu>
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
  // clang-format on
}
