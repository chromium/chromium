// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ChatRole} from '../context_hub.mojom-webui.js';

import type {TabGroupsElement} from './tab_groups.js';

export function getHtml(this: TabGroupsElement) {
  // clang-format off
  return html`
    <main id="tab-groups-view" class="split-view">
        <div class="left-panel">
            <section class="header-section">
                <h1>Tab Groups</h1>
                ${this.isGrouped_ ? html`
                    <div class="header-buttons">
                        <cr-button class="action-button"
                            id="default-grouping-button"
                            ?disabled="${!this.autoTabGroupsEnabled_ || this.isGrouping_}"
                            @click="${this.onDefaultGroupingClick_}">
                            Default Grouping
                        </cr-button>
                        <cr-button class="action-button"
                            id="ungroup-tabs-button"
                            ?disabled="${!this.autoTabGroupsEnabled_ || this.isGrouping_}"
                            @click="${this.onUngroupTabsClick_}">
                            Ungroup tabs
                        </cr-button>
                    </div>
                ` : html`
                    <cr-button class="action-button"
                        id="group-tabs-button"
                        ?disabled="${!this.autoTabGroupsEnabled_ || this.isGrouping_}"
                        @click="${this.onGroupTabsClick_}">
                        ${this.isGrouping_ ? 'Grouping...' : 'Group tabs'}
                    </cr-button>
                `}
            </section>

            <section class="content-section">
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
        </div>

        <div class="right-panel">
            <div class="chat-header">
                <h2>Chat</h2>
                <cr-button class="action-button"
                    id="clear-chat-history-button"
                    ?disabled="${!this.autoTabGroupsEnabled_ || this.isGrouping_}"
                    @click="${this.onClearChatHistoryClick_}">
                    Clear context
                </cr-button>
            </div>

            <div class="chat-messages" id="chat-messages">
                ${this.chatHistory_.map(msg => html`
                    <div class="message-bubble ${msg.role === ChatRole.kUser ? 'user' : 'assistant'}">
                        <div class="message-content">${msg.content}</div>
                    </div>
                `)}
            </div>

            <div class="chat-input-section">
                <cr-input id="group-input"
                    ?disabled="${!this.autoTabGroupsEnabled_ || this.isGrouping_}"
                    placeholder="${this.isGrouped_ ? 'Describe any further group changes...' : 'Enter prompt to group tabs...'}"
                    .value="${this.inputValue_}"
                    @value-changed="${this.onInputValueChanged_}"
                    @keydown="${this.onInputKeydown_}">
                </cr-input>
                <cr-icon-button class="send-button"
                    id="send-button"
                    iron-icon="cr:arrow-forward"
                    ?disabled="${this.isGrouping_ || !this.autoTabGroupsEnabled_ || !this.inputValue_.trim()}"
                    @click="${this.onGroupTabsClick_}">
                </cr-icon-button>

            </div>
        </div>
    </main>
  `;
  // clang-format on
}
