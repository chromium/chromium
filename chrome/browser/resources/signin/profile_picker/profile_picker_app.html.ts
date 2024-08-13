// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfilePickerAppElement} from './profile_picker_app.js';

export function getHtml(this: ProfilePickerAppElement) {
  return html`<!--_html_template_start_-->
${this.shouldDisplayVerticalBanners_ ? html`
  <img class="tangible-sync-style-left-banner" alt="">
  <img class="tangible-sync-style-right-banner" alt="">
` : ''}

<cr-view-manager id="viewManager">
  <profile-picker-main-view id="mainView" slot="view">
  </profile-picker-main-view>

  <cr-lazy-render-lit id="profileTypeChoice" .template="${() => html`
      <profile-type-choice slot="view"
          .profileThemeInfo="${this.newProfileThemeInfo}"
          .profileCreationInProgress="${this.profileCreationInProgress}">
      </profile-type-choice>`}">
  </cr-lazy-render-lit>

  <cr-lazy-render-lit id="profileSwitch"
      .template="${() => html`<profile-switch slot="view"></profile-switch>`}">
  </cr-lazy-render-lit>

  <if expr="chromeos_lacros">
    <cr-lazy-render-lit id="accountSelectionLacros"
        .template="${() => html`
          <account-selection-lacros slot="view"
              .profileThemeInfo="${this.newProfileThemeInfo}">
          </account-selection-lacros>`}">
    </cr-lazy-render-lit>
  </if>
</cr-view-manager>
<!--_html_template_end_-->`;
}
