// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ModuleListData} from './about_conflicts.js';

export function getHtml(data: ModuleListData) {
  // clang-format off
  return html`
<div id="container" class="vbox-container">
  <div id="top" class="wbox">
    <div class="section-header">
      <span class="section-header-title">${data.moduleCount}</span>
      <span>modules</span>
    </div>
  </div>
</div>

<div class="content">
  ${data.moduleList.length === 0 ? html`
    <div class="module-name no-modules">
      Unable to detect any modules loaded.
    </div>
  ` : html`
    <div>
      <table width="100%" cellpadding="0" cellspacing="0">
        <tr class="module-loaded">
          <td valign="top">
            <table cellpadding="2" cellspacing="0" border="0">
              <tr>
                <th role="columnheader"><span dir="ltr">Software</span></th>
                <th role="columnheader"><span dir="ltr">Signed by</span></th>
                <th role="columnheader"><span dir="ltr">Version</span></th>
                <th role="columnheader"><span dir="ltr">Code Id</span></th>
                <th role="columnheader"><span dir="ltr">Process types</span></th>
                <th role="columnheader"><span dir="ltr">Location</span></th>
              </tr>

              ${data.moduleList.map(item => html`
                <tr data-process="${item.process_types}" class="module">
                  <td valign="top" class="datacell">
                    <span dir="ltr" class="clearing nowrap">
                      ${item.description}
                    </span>
                  </td>
                  <td valign="top" class="datacell">
                    <span dir="ltr" class="nowrap">
                      ${item.digital_signer}
                    </span>
                  </td>
                  <td valign="top" class="datacell">
                    <span dir="ltr" class="nowrap">${item.version}</span>
                  </td>
                  <td valign="top" class="datacell">
                    <span dir="ltr" class="nowrap">${item.code_id}</span>
                  </td>
                  <td valign="top" class="datacell">
                    <span dir="ltr" class="nowrap">
                      ${item.process_types}
                    </span>
                  </td>
                  <td valign="top" class="datacell">
                    <span class="nowrap">
                      <span dir="ltr">${item.location}</span>
                      <strong>
                        <span dir="ltr">${item.name}</span>
                      </strong>
                      ${item.type_description ? html`
                        <span dir="ltr">(${item.type_description})</span>
                      ` : ''}
                    </span>
                  </td>
                </tr>
              `)}
            </table>
          </td>
        </tr>
      </table>
    </div>
  `}
</div>
  `;
  // clang-format on
}
