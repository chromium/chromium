// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {OnDeviceModelStatus} from './on_device_ai_browser_proxy.js';
import type {SettingsOnDeviceAiPageEntryElement} from './on_device_ai_page_entry.js';

export function getHtml(this: SettingsOnDeviceAiPageEntryElement) {
  return html`<!--_html_template_start_-->
<div id="modelRow">
  <cr-icon id="modelIcon" icon="${this.getIcon_()}" aria-hidden="true">
  </cr-icon>
  <div id="modelDetails">
    <div id="modelTitle">${this.capability.categoryTitle}</div>
    <div id="modelDescription" class="secondary">
      ${this.capability.description}
    </div>
    <div id="modelMetadata" class="secondary">
      ${this.getMetadataText_()}
    </div>
  </div>
  <cr-expand-button id="expand" no-hover ?expanded="${this.expanded_}"
      @expanded-changed="${this.onExpandedChanged_}"
      aria-label="${this.i18n(
          'onDeviceAiModelExpandA11yLabel', this.capability.categoryTitle)}">
  </cr-expand-button>
  <div class="separator"></div>
  ${this.capability.status === OnDeviceModelStatus.READY ? html`
    <cr-icon-button id="delete" iron-icon="settings20:delete"
        title="$i18n{delete}" @click="${this.onDeleteClick_}"
        aria-label="${this.i18n(
            'onDeviceAiModelDeleteA11yLabel', this.capability.categoryTitle)}">
    </cr-icon-button>
  ` : html`
    <cr-icon id="statusIcon" icon="${this.getStatusIcon_()}" aria-hidden="true">
    </cr-icon>
  `}
</div>
<cr-collapse id="collapse" ?opened="${this.expanded_}">
  <ul id="modelDetailsList">
    ${this.capability.details.map(item => html`
      <li>
        <cr-icon icon="cr:check" aria-hidden="true"></cr-icon>
        <span>${item}</span>
      </li>
    `)}
  </ul>
</cr-collapse>
<!--_html_template_end_-->`;
}
