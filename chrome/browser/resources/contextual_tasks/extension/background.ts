// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    async (message: any, sender: chrome.runtime.MessageSender) => {
      const urlMatchesAllowList = function(origin: string) {
        try {
          const url = new URL(origin);
          if (url.protocol !== 'https:') {
            return false;
          }
          const host = url.hostname;
          return host === 'google.com' || host === 'www.google.com' ||
              host.endsWith('.borg.google.com') ||
              host.endsWith('.corp.google.com') ||
              host.endsWith('.prod.google.com');
        } catch {
          return false;
        }
      };

      if (!sender.origin || !URL.parse(sender.origin) ||
          !urlMatchesAllowList(sender.origin)) {
        throw new Error('Unauthorized sender origin');
      }

      if (!sender.documentId) {
        throw new Error('Missing documentId');
      }

      if (message && message.type === 'contextualTasksPrivate.getState') {
        const state =
            await chrome.contextualTasksPrivate.getState(sender.documentId);
        return {state};
      }

      if (message &&
          message.type === 'contextualTasksPrivate.launchPanelInNewTab' &&
          message.args) {
        const details = message.args;
        if (!details.targetUrl || !URL.parse(details.targetUrl)) {
          throw new Error('Invalid targetUrl');
        }
        if (!details.aimUrl || !URL.parse(details.aimUrl)) {
          throw new Error('Invalid aimUrl');
        }
        return await chrome.contextualTasksPrivate.launchPanelInNewTab({
          targetUrl: details.targetUrl,
          aimUrl: details.aimUrl,
          documentId: sender.documentId,
        });
      }

      throw new Error(`Unhandled message: ${JSON.stringify(message)}`);
    });
