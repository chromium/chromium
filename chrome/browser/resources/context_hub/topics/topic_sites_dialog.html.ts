// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicSitesDialogElement} from './topic_sites_dialog.js';

export function getHtml(this: TopicSitesDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <div slot="title">
    <div id="title">${this.topic?.title || ''}</div>
    <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
    <!-- strings are added. -->
    <div id="subtitle">Sites you've visited related to this topic</div>
  </div>
  <div slot="body">
    <ul id="siteList">
      ${this.sites_.map((site, index) => html`
        <li ?rejected="${this.feedbackEnabled_ && this.isRejected_(site)}">
          <button class="site" data-index="${index}"
              @click="${this.onSiteClick_}">
            <span class="favicon"
                style="background-image: ${this.getFavicon_(site.url)}">
            </span>
            <span class="site-title">${site.title || site.domain}</span>
            ${site.title ? html`
              <span class="site-domain">${site.domain}</span>
            ` : ''}
          </button>
          ${this.feedbackEnabled_ ? html`
            <!-- Temporary Fishfood Feedback -->
            ${this.isRejected_(site) ? html`
              <span class="rejected-label" aria-hidden="true">
                Doesn't belong
              </span>
            ` : ''}
            <cr-icon-button class="reject" data-index="${index}"
                iron-icon="${this.isRejected_(site) ?
                    'cr:cancel-filled' : 'cr:close'}"
                title="${this.isRejected_(site) ? 'Undo' : 'Doesn\'t belong'}"
                aria-label="${this.getRejectAriaLabel_(site)}"
                aria-pressed="${this.isRejected_(site)}"
                @click="${this.onRejectClick_}">
            </cr-icon-button>
          ` : ''}
        </li>
      `)}
    </ul>
  </div>
  <div slot="button-container">
    <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
    <!-- strings are added. -->
    <cr-button id="openAllTabs" class="cancel-button"
        @click="${this.onOpenAllTabsClick_}">
      Open all tabs
    </cr-button>
    <cr-button id="ok" class="action-button" @click="${this.onOkClick_}">
      OK
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
