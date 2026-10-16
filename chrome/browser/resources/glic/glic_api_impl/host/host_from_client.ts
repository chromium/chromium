// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the client, usually passing them on
// to the browser via mojo.

import type {WebClientHandlerInterface} from '../../glic.mojom-webui.js';
import type {TabContextOptions} from '../../glic_api/glic_api.js';
import {replaceProperties} from '../conversions.js';
import type {                         //
             GlicException,           //
             ImageBytesResultPrivate, //
             RgbaImage,               //
             TabContextResultPrivate, //
             WebClientHost,           //
} from '../request_types.js';
import {exceptionFromTransferable} from '../request_types.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {PostMessageHandler} from '../transport/post_message_transport.js';

import {                        //
  bitmapN32ToRGBAImage,         //
  idFromClient,                 //
  imageBytesResultToClient,     //
  tabContextOptionsFromClient,  //
  tabContextToClient,           //
} from './conversions.js';
import type {GlicApiHost} from './glic_api_host.js';

/**
 * Handles all requests to the host.
 *
 * Each function is a message handler, automatically called when the host
 * receives a message with the corresponding request name.
 *
 * Any new state or function that's not a handler should be added to
 * `GlicApiHost`.
 */
export class HostMessageHandler implements PostMessageHandler<WebClientHost> {
  // Reminder: Don't add more state here! See `HostMessageHandler`'s comment.
  constructor(
      private handler: WebClientHandlerInterface, private host: GlicApiHost) {}

  destroy() {}

  webClientInitialized(request: {success: boolean, exception?: GlicException}) {
    // The webview may have been re-shown by webui, having previously been
    // opened by the browser. In that case, show the guest frame again.

    if (request.exception) {
      console.warn(exceptionFromTransferable(request.exception));
    }

    if (request.success) {
      this.host.webClientInitialized();
    } else {
      this.host.webClientInitializeFailed();
    }
  }

  async getContextFromFocusedTab(
      request: {options: TabContextOptions}, extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromFocusedTab(
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async getContextFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromTab(
            idFromClient(request.tabId),
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async getImageBytesFromTab(
      request: {tabId: string, documentId: string, domNodeId: number},
      extras: ResponseExtras):
      Promise<{result: ImageBytesResultPrivate | null}> {
    const {result: {errorReason, imageBytes}} =
        await this.handler.getImageBytesFromTab(
            idFromClient(request.tabId), request.documentId, request.domNodeId);
    if (!imageBytes) {
      throw new Error(`getImageBytes failed: ${errorReason}`);
    }
    return {
      result: imageBytesResultToClient(imageBytes, extras),
    };
  }

  async getUserProfileInfo(_request: void, extras: ResponseExtras) {
    const {profileInfo: mojoProfileInfo} =
        await this.handler.getUserProfileInfo();
    if (!mojoProfileInfo) {
      return {};
    }

    let avatarIcon: RgbaImage|undefined;
    bitmapN32ToRGBAImage;
    if (mojoProfileInfo.avatarIcon) {
      avatarIcon = bitmapN32ToRGBAImage(mojoProfileInfo.avatarIcon);
      if (avatarIcon) {
        extras.addTransfer(avatarIcon.dataRGBA);
      }
    }
    return {profileInfo: replaceProperties(mojoProfileInfo, {avatarIcon})};
  }
}
