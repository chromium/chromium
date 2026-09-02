// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import type {MemoryBanksElement} from './memory_banks.js';

export function getHtml(this: MemoryBanksElement, entry: MemoryBankEntry) {
  return html`
    <a class="card ${this.isSelected(entry.id) ? 'selected' : ''}"
        href="${entry.url}" target="_blank">
      <cr-checkbox class="card-checkbox no-label"
          data-id="${entry.id}"
          ?checked="${this.isSelected(entry.id)}"
          @change="${this.onCheckboxChange}"
          @click="${this.onCheckboxClick}">
      </cr-checkbox>
      ${
      entry.type === EntryType.kTextSelection ? html`
        <div class="card-body">
          <p class="text-preview">"${entry.selectedText || ''}"</p>
        </div>
      ` :
                                                html`
        <div class="card-body tab-type">
          <cr-icon icon="cr:draft-filled"></cr-icon>
        </div>
      `}
      <div class="card-footer">
        <div class="footer-main">
          <div class="favicon"
              style="background-image: ${
                  getFaviconForPageURL(entry.url, true)}">
          </div>
          <div class="meta-text">
            <span class="card-title" title="${entry.tabTitle}">${
        entry.tabTitle}</span>
            <span class="card-date">
              ${
        this.convertMojoTimeToDate(entry.timestamp)
            .toLocaleDateString(undefined, {
              month: 'short',
              day: 'numeric',
              year: 'numeric',
            })}
            </span>
          </div>
          <cr-icon-button class="card-more-btn" iron-icon="cr:more-vert"
              title="More actions"
              @click="${(e: MouseEvent) => this.onMoreActionsClick_(entry, e)}">
          </cr-icon-button>
        </div>
        ${
        entry.tags && entry.tags.length > 0 ? html`
          <div class="card-tags">
            ${entry.tags.map(tag => html`
              <span class="tag-pill" title="${tag}">${tag}</span>
            `)}
          </div>
        ` :
                                              ''}
      </div>
    </a>
  `;
}
