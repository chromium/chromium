// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ChatRole} from '../context_hub.mojom-webui.js';

import type {MemoryBankChatElement} from './memory_bank_chat.js';

export function getHtml(this: MemoryBankChatElement) {
  return html`
    <main id="memory-bank-chat-view" class="chat-container">
      <div class="chat-header">
        <h2>Memory Bank Chat</h2>
        <cr-button class="action-button"
            id="clear-chat-history-button"
            aria-label="Clear chat"
            ?disabled="${
      this.isLoading_ || (this.chatHistory_.length === 0 && !this.inputValue_)}"
            @click="${this.onClearChatHistoryClick_}">
          Clear chat
        </cr-button>
      </div>

      <div class="chat-messages" id="chat-messages">
        ${
      this.chatHistory_.map(
          msg => html`
          <div class="message-bubble ${
              msg.role === ChatRole.kUser ? 'user' : 'assistant'}">
            <div class="message-content">${msg.content}</div>
          </div>
        `)}
        ${
      this.isLoading_ ? html`
          <div class="message-bubble assistant loading-bubble"
              role="status"
              aria-live="polite">
            <div class="spinner"></div>
            <span>Generating response...</span>
          </div>
        ` :
                        ''}
      </div>

      ${
      this.attachedChips_.size > 0 ? html`
        <div class="attached-entries-bar">
          <span class="attached-label">Attached context:</span>
          <div class="attached-chips">
            ${Array.from(this.attachedChips_.values()).map(chip => html`
              <div class="entry-chip"
                  title="${chip.title}">
                <span class="chip-title">${chip.title}</span>
                <cr-icon-button class="chip-remove-btn"
                    data-id="${chip.id}"
                    iron-icon="cr:close"
                    aria-label="Remove attached memory"
                    @click="${this.onRemoveAttachedChipClick_}">
                </cr-icon-button>
              </div>
            `)}
          </div>
          <cr-button class="clear-attached-btn"
              @click="${this.onClearAttachedChipsClick_}">
            Clear all
          </cr-button>
        </div>
      ` :
                                     ''}

      <div class="chat-input-section">
        ${
      this.showMentionMenu_ ?
          html`
          <div class="mention-menu" id="mention-menu" role="listbox"
              aria-label="Memory suggestions">
            <div class="mention-menu-header">
              ${this.getMentionHeader_()}
            </div>
            ${
              this.mentionSuggestions_.map(
                  (item, index) => html`
              <div class="mention-item ${
                      index === this.highlightedMentionIndex_ ? 'highlighted' :
                                                                ''} ${
                      item.kind !== 'individual-entry' ? 'mention-group-item' :
                                                         ''}"
                  role="option"
                  data-index="${index}"
                  title="${item.label}"
                  aria-selected="${index === this.highlightedMentionIndex_}"
                  @mouseenter="${this.onMentionItemMouseenter_}"
                  @click="${this.onMentionItemClick_}">
                <span class="mention-item-title">${item.label}</span>
                ${
                      item.description ? html`
                  <span class="mention-item-description"
                      >${item.description}</span>
                ` :
                                         ''}
                ${
                      item.kind === 'qualifier-filter' ? html`
                  <span class="mention-item-badge"
                      >${this.getEntryCountLabel_(item.chips.length)}</span>
                ` :
                                                         ''}
              </div>
            `)}
            ${
              this.mentionSuggestions_.length === 0 ? html`
              <div class="mention-empty">No matching items found.</div>
            ` :
                                                      ''}
          </div>
        ` :
          ''}

        <cr-input id="chat-input"
            ?disabled="${this.isLoading_}"
            placeholder=
                "Ask a question about your memories... (type @ to attach)"
            aria-label="Ask a question about your memories"
            .value="${this.inputValue_}"
            @value-changed="${this.onInputValueChanged_}"
            @keydown="${this.onInputKeydown_}">
        </cr-input>
        <cr-icon-button class="send-button"
            id="send-button"
            iron-icon="cr:arrow-forward"
            aria-label="Send message"
            ?disabled="${this.isLoading_ || !this.inputValue_.trim()}"
            @click="${this.onSendMessageClick_}">
        </cr-icon-button>
      </div>
    </main>
  `;
}
