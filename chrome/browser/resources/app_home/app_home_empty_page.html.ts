// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppHomeEmptyPageElement} from './app_home_empty_page.js';

export function getHtml(this: AppHomeEmptyPageElement) {
  return html`<!--_html_template_start_-->
<div class="container">
  <img src="chrome://resources/images/apps_home_empty_238x170.svg">
    <p>$i18n{appAppearanceLabel}</p>
    <a href="https://support.google.com/chrome?p=install_web_apps"
        target="_blank">
      <cr-button>$i18n{learnToInstall}</cr-button>
    </a>
</div>
<!--_html_template_end_-->`;
}
