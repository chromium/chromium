// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppListElement} from './app_list.js';

export function getHtml(this: AppListElement) {
  return html`<!--_html_template_start_-->
${this.apps_.length > 0 ? html`
  <div id="container">
    ${this.apps_.map(item => html`
      <app-item class="item" id="${item.id}" .appInfo="${item}"
          .ariaLabel="${item.name}${this.notLocallyInstalledString_(
              item.isLocallyInstalled, '$i18n{notInstalled}')}"
          role="button" tabindex="0">
      </app-item>
    `)}
  </div>
` : html`
  <app-home-empty-page></app-home-empty-page>
`}
<!--_html_template_end_-->`;
}
