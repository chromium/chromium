// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allowedUrlPatterns = [
  'https://gemini.google.com/*',
  'https://*.google.com/*',
];
chrome.runtime.onMessageExternal.addListener(
    async (message: any, sender: chrome.runtime.MessageSender) => {
      const urlMatchesAllowList = function(url: string) {
        return allowedUrlPatterns.some(pattern => {
          return new URLPattern(pattern).test(url);
        });
      };

      if (!sender.origin || !URL.parse(sender.origin) ||
          !urlMatchesAllowList(sender.origin)) {
        throw new Error(`bad sender origin`);
      }

      if (!sender.documentId) {
        throw new Error('missing documentId');
      }

      if (message && message.type === 'glicPrivate.getState') {
        const state = await chrome.glicPrivate.getState(sender.documentId);
        return {state};
      }

      if (message && message.type === 'glicPrivate.invoke' && message.args) {
        let details = message.args;
        // Support old format where args was an array of arguments.
        if (Array.isArray(message.args)) {
          if (message.args.length === 0) {
            throw new Error('missing invoke details');
          }
          details = message.args[0];
        }

        if (!details.invocationSource) {
          throw new Error('missing invocationSource');
        }
        if (!details.promptId &&
            details.invocationSource !== 'promotion-page') {
          throw new Error('missing promptId');
        }
        return await chrome.glicPrivate.invoke({
          promptId: details.promptId,
          invocationSource: details.invocationSource,
          documentId: sender.documentId,
          inNewTab: details.inNewTab,
        });
      }

      if (message && message.type === 'glicPrivate.hasConversation') {
        if (!message.args || !message.args.conversationId) {
          return {error: 'missing conversationId'};
        }
        const isPresent = await chrome.glicPrivate.hasConversation(
            message.args.conversationId);
        return {isPresent};
      }

      if (message &&
          message.type === 'glicPrivate.activateTabWithConversation') {
        if (!message.args || !message.args.conversationId) {
          return {error: 'missing conversationId'};
        }
        await chrome.glicPrivate.activateTabWithConversation(
            message.args.conversationId);
        return;
      }

      throw new Error(`Unhandled message: ${JSON.stringify(message)}`);
    });
