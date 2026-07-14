// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabGroupsElement} from './tab_groups.js';

export function getHtml(this: TabGroupsElement) {
  // clang-format off
  return html`
    <main id="tab-groups-view">
        <section class="header-section">
            <h1>Tab Groups</h1>
        </section>

        <section class="input-section">
            <cr-input id="group-input" disabled
                placeholder="Enter prompt to group tabs..."
                .value="${this.inputValue_}"
                @value-changed="${this.onInputValueChanged_}">
            </cr-input>
            <cr-button class="action-button"
                ?disabled="${this.isGrouping_ || !this.autoTabGroupsEnabled_}"
                @click="${this.onGroupTabsClick_}">
                ${this.isGrouping_ ? 'Grouping...' :
                  (this.isGrouped_ ? 'Ungroup tabs' : 'Group tabs')}
            </cr-button>
        </section>

        <section>
            ${this.isGrouping_ ? html`
                <div class="loading-container">
                    <div class="spinner"></div>
                    <span>Clustering tabs with Gemini...</span>
                </div>
            ` : (this.isGrouped_ ? html`
                <div class="groups-container">
                    ${this.groups_.map((group, index) => html`
                        <div class="group-card">
                            <cr-expand-button
                                data-index="${index}"
                                ?expanded="${group.expanded}"
                                @expanded-changed="${this.onGroupExpandedChanged_}">
                                <div class="group-header">
                                    <span class="group-label">${group.label}</span>
                                    <span class="group-count">(${group.tabs.length} tabs)</span>
                                </div>
                            </cr-expand-button>
                            <cr-collapse ?opened="${group.expanded}">
                                <div class="group-tabs-list">
                                    ${group.tabs.map(tab => html`
                                        <div class="group-tab-item"
                                            data-id="${tab.id}"
                                            @click="${this.onTabClick_}">
                                            <div class="tab-title">${tab.title}</div>
                                            <div class="tab-url">${tab.url}</div>
                                        </div>
                                    `)}
                                </div>
                            </cr-collapse>
                        </div>
                    `)}
                </div>

                ${this.ungroupedTabs_.length > 0 ? html`
                    <div class="ungrouped-section">
                        <h2>Ungrouped tabs</h2>
                        <div class="grid">
                            ${this.ungroupedTabs_.map(tab => html`
                                <div class="tab-card"
                                    data-id="${tab.id}"
                                    @click="${this.onTabClick_}">
                                    <div class="tab-title">${tab.title}</div>
                                </div>
                            `)}
                        </div>
                    </div>
                ` : ''}
            ` : html`
                <div class="grid">
                    ${this.tabs_.map(tab => html`
                        <div class="tab-card"
                                    data-id="${tab.id}"
                                    @click="${this.onTabClick_}">
                            <div class="tab-title">${tab.title}</div>
                        </div>
                    `)}
                </div>
            `)}
        </section>
    </main>
  `;
  // clang-format on
}
