// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

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

    this.chatHistory_ = [
      ...this.chatHistory_,
      {role: ChatRole.kUser, content: command},
    ];
    this.scrollToBottom_();

    try {
      const {response} =
          await browserProxyFactory.getInstance().handler.askGeminiWithContext(
              command, []);

      const assistantContent =
          response?.content.trim() || 'No response from Gemini.';
      this.chatHistory_ = [
        ...this.chatHistory_,
        {role: ChatRole.kAssistant, content: assistantContent},
      ];
    } catch (e) {
      console.error('Failed to process request with Gemini:', e);
      this.chatHistory_ = [
        ...this.chatHistory_,
        {
          role: ChatRole.kAssistant,
          content: 'Failed to process request with Gemini.',
        },
      ];
    } finally {
      this.isLoading_ = false;
      this.scrollToBottom_();
    }
  }

  protected onClearChatHistoryClick_() {
    if (this.isLoading_) {
      return;
    }
    this.chatHistory_ = [];
    this.inputValue_ = '';
  }

  private scrollToBottom_() {
    this.updateComplete.then(() => {
      const chatMessages =
          this.shadowRoot?.querySelector<HTMLElement>('#chat-messages');
      if (chatMessages) {
        chatMessages.scrollTop = chatMessages.scrollHeight;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-bank-chat': MemoryBankChatElement;
  }
}

customElements.define(MemoryBankChatElement.is, MemoryBankChatElement);
