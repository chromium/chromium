// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, ChatRole} from '../context_hub.mojom-webui.js';
import type {ChatMessage} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_bank_chat.css.js';
import {getHtml} from './memory_bank_chat.html.js';

export class MemoryBankChatElement extends CrLitElement {
  static get is() {
    return 'memory-bank-chat';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      chatHistory_: {type: Array},
      inputValue_: {type: String},
      isLoading_: {type: Boolean},
    };
  }

  protected accessor chatHistory_: ChatMessage[] = [];
  protected accessor inputValue_: string = '';
  protected accessor isLoading_: boolean = false;
  protected maxChatHistoryTurns_: number =
      loadTimeData.getInteger('kMaxMemoryBankChatHistoryTurns');

  override connectedCallback() {
    super.connectedCallback();
    this.fetchChatHistory_();
  }

  private trimChatHistory_(history: ChatMessage[]): ChatMessage[] {
    if (this.maxChatHistoryTurns_ > 0 &&
        history.length > this.maxChatHistoryTurns_) {
      return history.slice(-this.maxChatHistoryTurns_);
    }
    return history;
  }

  private appendChatTurn_(role: ChatRole, content: string) {
    this.chatHistory_ = this.trimChatHistory_([
      ...this.chatHistory_,
      {role, content},
    ]);
    this.scrollToBottom_();
  }

  private async fetchChatHistory_() {
    try {
      const {history} = await browserProxyFactory.getInstance()
                            .handler.getMemoryBankChatHistory();
      if (history && history.length > 0) {
        this.chatHistory_ = this.trimChatHistory_(history);
        this.scrollToBottom_();
      }
    } catch (e) {
      console.error('Failed to load memory bank chat history:', e);
    }
  }

  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.inputValue_ = e.detail.value;
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter' && this.inputValue_.trim().length > 0) {
      this.onSendMessageClick_();
    }
  }

  protected async onSendMessageClick_() {
    if (this.isLoading_) {
      return;
    }

    const command = this.inputValue_.trim();
    if (!command) {
      return;
    }

    this.isLoading_ = true;
    this.inputValue_ = '';
    this.appendChatTurn_(ChatRole.kUser, command);

    try {
      const {response} =
          await browserProxyFactory.getInstance().handler.askGeminiWithContext(
              command, []);

      const assistantContent =
          response?.content.trim() || 'No response from Gemini.';
      this.appendChatTurn_(ChatRole.kAssistant, assistantContent);
    } catch (e) {
      console.error('Failed to process request with Gemini:', e);
      this.appendChatTurn_(
          ChatRole.kAssistant, 'Failed to process request with Gemini.');
    } finally {
      this.isLoading_ = false;
    }
  }

  protected async onClearChatHistoryClick_() {
    if (this.isLoading_) {
      return;
    }
    await browserProxyFactory.getInstance()
        .handler.clearMemoryBankChatHistory();
    this.chatHistory_ = [];
    this.inputValue_ = '';
  }

  private async scrollToBottom_() {
    await this.updateComplete;
    const chatMessages =
        this.shadowRoot?.querySelector<HTMLElement>('#chat-messages');
    if (chatMessages) {
      chatMessages.scrollTop = chatMessages.scrollHeight;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-bank-chat': MemoryBankChatElement;
  }
}

customElements.define(MemoryBankChatElement.is, MemoryBankChatElement);
