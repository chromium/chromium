// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionPageCallbackRouter, ExtensionPageHandlerFactory, ExtensionPageHandlerRemote} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import type {ExtensionPageInterface} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import {getArrayBufferFromBigBuffer} from 'chrome://contextual-tasks/utils.js';

type ProtoWrapper = Parameters<ExtensionPageInterface['postSearchMessage']>[0];

declare global {
  namespace chrome {
    namespace runtime {
      const onConnectExternal: PortEvent;
    }
  }
}

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

interface SearchChannelMessage {
  type?: string;
  payload?: number[];
}

const CHROME_SEARCH_CHANNEL_NAME = 'chrome-search-channel';
const MAX_MESSAGE_BYTES = 1024 * 1024;

const portsByDocumentId = new Map<string, chrome.runtime.Port>();
let extensionPageHandler: ExtensionPageHandlerRemote|null = null;
let extensionPageRouter: ExtensionPageCallbackRouter|null = null;

function postProtoMessageToPort(
    documentId: string, port: chrome.runtime.Port,
    message: ProtoWrapper): boolean {
  try {
    const buffer = getArrayBufferFromBigBuffer(message.smuggled);
    if (buffer.byteLength === 0) {
      return false;
    }
    port.postMessage({
      type: CHROME_SEARCH_CHANNEL_NAME,
      payload: Array.from(new Uint8Array(buffer)),
    });
    return true;
  } catch {
    handleSearchPortDisconnect(documentId, port);
    return false;
  }
}

function getExtensionPageHandler(): ExtensionPageHandlerRemote {
  if (!extensionPageHandler) {
    extensionPageRouter = new ExtensionPageCallbackRouter();
    extensionPageRouter.postSearchMessage.addListener(
        (message: ProtoWrapper, documentId: string|null) => {
          if (!documentId) {
            return;
          }
          const port = portsByDocumentId.get(documentId);
          if (!port) {
            return;
          }
          postProtoMessageToPort(documentId, port, message);
        });
    extensionPageHandler = new ExtensionPageHandlerRemote();
    const factory = ExtensionPageHandlerFactory.getRemote();
    factory.createExtensionPageHandler(
        extensionPageRouter.$.bindNewPipeAndPassRemote(),
        extensionPageHandler.$.bindNewPipeAndPassReceiver());
    extensionPageHandler.onConnectionError.addListener(() => {
      extensionPageHandler = null;
      extensionPageRouter = null;
    });
  }
  return extensionPageHandler;
}

function handleSearchPortMessage(
    documentId: string, port: chrome.runtime.Port, message: unknown) {
  if (portsByDocumentId.get(documentId) !== port) {
    return;
  }
  if (!message || typeof message !== 'object') {
    return;
  }
  const searchMsg = message as SearchChannelMessage;
  if (searchMsg.type !== CHROME_SEARCH_CHANNEL_NAME ||
      !Array.isArray(searchMsg.payload) || searchMsg.payload.length === 0 ||
      searchMsg.payload.length > MAX_MESSAGE_BYTES) {
    return;
  }
  getExtensionPageHandler().onWebviewMessage(searchMsg.payload, documentId);
}

function handleSearchPortDisconnect(
    documentId: string, port: chrome.runtime.Port) {
  if (portsByDocumentId.get(documentId) === port) {
    portsByDocumentId.delete(documentId);
  }
  port.disconnect();
}

async function sendSearchPortHandshake(
    documentId: string, port: chrome.runtime.Port) {
  try {
    const {message} =
        await getExtensionPageHandler().getHandshakeMessage(documentId);
    if (portsByDocumentId.get(documentId) !== port) {
      return;
    }
    if (!postProtoMessageToPort(documentId, port, message)) {
      handleSearchPortDisconnect(documentId, port);
    }
  } catch {
    handleSearchPortDisconnect(documentId, port);
  }
}

function urlMatchesAllowList(origin: string): boolean {
  try {
    const url = new URL(origin);
    if (url.protocol !== 'https:') {
      return false;
    }
    const host = url.hostname;
    return host === 'google.com' || host === 'www.google.com' ||
        host.endsWith('.borg.google.com') ||
        host.endsWith('.corp.google.com') || host.endsWith('.prod.google.com');
  } catch {
    return false;
  }
}

chrome.runtime.onConnectExternal.addListener((port: chrome.runtime.Port) => {
  // Only accept connections on the dedicated search channel.
  if (port.name !== CHROME_SEARCH_CHANNEL_NAME) {
    port.disconnect();
    return;
  }

  // Verify the connecting frame belongs to an allowlisted Google origin.
  if (!port.sender?.origin || !urlMatchesAllowList(port.sender.origin)) {
    port.disconnect();
    return;
  }

  // Require a valid extension documentId to route messages to the right tab
  // or side panel WebContents in the browser process.
  const documentId = port.sender.documentId;
  if (typeof documentId !== 'string' || documentId.length === 0) {
    port.disconnect();
    return;
  }

  // If this document already has an open port, replace and disconnect the old
  // port so stale onDisconnect events cannot drop the new connection.
  const existingPort = portsByDocumentId.get(documentId);
  if (existingPort && existingPort !== port) {
    handleSearchPortDisconnect(documentId, existingPort);
  }
  portsByDocumentId.set(documentId, port);

  // Forward incoming search messages and clean up on port disconnect.
  port.onMessage.addListener((message: unknown) => {
    handleSearchPortMessage(documentId, port, message);
  });
  port.onDisconnect.addListener(() => {
    handleSearchPortDisconnect(documentId, port);
  });

  // Kick off the initial search handshake with the browser process.
  sendSearchPortHandshake(documentId, port);
});

chrome.runtime.onMessageExternal.addListener(
    async (message: ExtensionMessage, sender: chrome.runtime.MessageSender) => {
      if (!sender.origin || !URL.parse(sender.origin) ||
          !urlMatchesAllowList(sender.origin)) {
        return {error: 'Unauthorized sender origin'};
      }

      if (!sender.documentId) {
        return {error: 'Missing documentId'};
      }

      if (message.type === 'contextualTasksPrivate.getState') {
        try {
          const state =
              await chrome.contextualTasksPrivate.getState(sender.documentId);
          return {state};
        } catch (err: any) {
          return {error: err.message || 'Unknown error'};
        }
      }

      if (message.type === 'contextualTasksPrivate.launchPanelInNewTab' &&
          message.args) {
        const details = message.args;
        if (!details.targetUrl || !URL.parse(details.targetUrl)) {
          return {error: 'Invalid targetUrl'};
        }
        const aimParams = details.aimParams || {};
        try {
          // LINT.IfChange(AimParamsCall)
          const result =
              await chrome.contextualTasksPrivate.launchPanelInNewTab({
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
          return result;
        } catch (err: any) {
          return {error: err.message || 'Unknown error'};
        }
      }

      return {error: `Unhandled message: ${JSON.stringify(message)}`};
    });
