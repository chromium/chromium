// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allowedUrlPatterns = ['https://*gemini*.google.com/*'];
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

      if (message && message.type === 'glicPrivate.getState') {
        const state = await chrome.glicPrivate.getState();
        return {state};
      } else {
        throw new Error(`Unhandled message: ${JSON.stringify(message)}`);
      }
    });
