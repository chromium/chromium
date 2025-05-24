// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ComponentsData} from './components.js';

export function getHtml(data: ComponentsData) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <div id="top">
    <div class="section-header">
      <span class="section-header-title">$i18n{componentsTitle}</span>
      <span class="section-header-title"
          ?hidden="${data.components.length === 0}">
        (${data.components.length})
      </span>
    </div>
  </div>

  <div class="content">
    <div class="component-name no-components"
        ?hidden=${data.components.length > 0}">
      <div>$i18n{noComponents}</div>
    </div>

    <div ?hidden="${data.components.length === 0}">
      ${data.components.map(item => html`
        <div class="component">
          <div class="component-enabled">
            <div class="component-text">
              <div>
                <span class="component-name" dir="ltr">${item.name}</span>
                <span>
                  - <span>$i18n{componentVersion}</span>
                    <span dir="ltr" id="version-${item.id}">
                      ${item.version}
                    </span>
                </span>
              </div>
            </div>
          </div>
          <div class="component-text">
            <span>$i18n{statusLabel}</span>
            -
            <span id="status-${item.id}">${item.status}<span>
          </div>
          <div class="component-actions">
            <button class="button-check-update" guest-disabled id="${item.id}">
              $i18n{checkUpdate}
            </button>
          </div>
        </div>
      `)}
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
