// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WelcomeAppElement} from './welcome_app.js';

export function getHtml(this: WelcomeAppElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" ?hidden="${!this.modulesInitialized_}">
  <landing-view id="step-landing" slot="view" class="active"></landing-view>
</cr-view-manager>
<cr-toast duration="3000">
  <div>$i18n{defaultBrowserChanged}</div>
</cr-toast>
<!--_html_template_end_-->`;
}
