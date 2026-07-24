// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// LINT.IfChange(AimParams)
interface AimParams {
  ntc?: string;
  mstk?: string;
  aioh?: string;
  csuir?: string;
  ved?: string;
  cs?: string;
  sxsrf?: string;
  ei?: string;
  q?: string;
}
// LINT.ThenChange(//chrome/common/extensions/api/contextual_tasks_private.webidl:AimParams)

interface ExtensionMessage {
  type?: string;
  args?: {targetUrl?: string, aimParams?: AimParams};
}

chrome.runtime.onMessageExternal.addListener(
    async (message: ExtensionMessage, sender: chrome.runtime.MessageSender) => {
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
        const aimParams = details.aimParams || {};
        // LINT.IfChange(AimParamsCall)
        return await chrome.contextualTasksPrivate.launchPanelInNewTab({
          aimParams: {
            ntc: aimParams.ntc ?? '',
            mstk: aimParams.mstk ?? '',
            aioh: aimParams.aioh ?? '',
            csuir: aimParams.csuir ?? '',
            ved: aimParams.ved ?? '',
            cs: aimParams.cs ?? '',
            sxsrf: aimParams.sxsrf ?? '',
            ei: aimParams.ei ?? '',
            q: aimParams.q ?? '',
          },
          targetUrl: details.targetUrl,
          documentId: sender.documentId,
        });
        // LINT.ThenChange(//chrome/common/extensions/api/contextual_tasks_private.webidl:AimParams)
      }

      throw new Error(`Unhandled message: ${JSON.stringify(message)}`);
    });
