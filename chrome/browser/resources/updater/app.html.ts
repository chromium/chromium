// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UpdaterAppElement} from './app.js';

export function getHtml(this: UpdaterAppElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<header>
  <div class="header-bar">
    <div id="logo"></div>
    <h1>$i18n{title}</h1>
  </div>
</header>
<div id="content">
  <div>
    <h2>$i18n{updaterStateTitle}</h2>
    <updater-state .userUpdaterState="${this.userUpdaterState}"
        .systemUpdaterState="${this.systemUpdaterState}"
        .error="${this.updaterStateError}">
    </updater-state>
  </div>
  <div>
    <h2>$i18n{installedAppsTitle}</h2>
    <app-list .apps="${this.apps}" .error="${this.appStateError}"></app-list>
  </div>
  <div>
    <h2>Enterprise Policies</h2>
    <enterprise-policy-table .policies="${this.policies}">
    </enterprise-policy-table>
  </div>
  <div>
    <h2>$i18n{eventListTitle}</h2>
    <event-list .messages="${this.messages}"></event-list>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
