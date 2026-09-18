// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacyGuideWelcomeFragmentElement} from './privacy_guide_welcome_fragment.js';

export function getHtml(this: PrivacyGuideWelcomeFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="welcome-completion-header">
  <picture>
    <source
        srcset="./images/privacy_guide/welcome_banner_dark.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/welcome_banner.svg">
  </picture>
  <h2 class="welcome-completion-header-label" tabindex="-1">
    $i18n{privacyGuideWelcomeCardHeader}
  </h2>
  <div class="cr-secondary-text">$i18n{privacyGuideWelcomeCardSubHeader}</div>
</div>
<div class="footer">
  <cr-button class="action-button" id="startButton"
      @click="${this.onStartButtonClick_}">
    $i18n{privacyGuideNextButton}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
