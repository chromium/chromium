// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBankChatElement} from './memory_bank_chat.js';

export function getHtml(this: MemoryBankChatElement) {
  return html`
    <main id="memory-bank-chat-view" class="chat-container">
      <div class="chat-header">
        <h1>Memory Bank Chat</h1>
        <cr-button class="action-button"
            id="clear-chat-button"
            aria-label="Clear chat">
          Clear chat
        </cr-button>
      </div>

      <div class="chat-messages" id="chat-messages">
      </div>

      <div class="chat-input-section">
        <cr-input id="chat-input"
            placeholder="Ask about your memory banks..."
            aria-label="Ask about your memory banks">
        </cr-input>
        <cr-icon-button class="send-button"
            id="send-button"
            iron-icon="cr:arrow-forward"
            aria-label="Send message">
        </cr-icon-button>
      </div>
    </main>
  `;
}
