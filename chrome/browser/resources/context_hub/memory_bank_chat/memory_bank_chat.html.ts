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
        <h1>Memory Bank Chat</h1>
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

      <div class="chat-input-section">
        <cr-input id="chat-input"
            ?disabled="${this.isLoading_}"
            placeholder="Ask a question about your memories..."
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
