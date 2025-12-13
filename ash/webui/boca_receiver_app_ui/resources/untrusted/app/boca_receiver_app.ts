// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

import {ConnectionClosedReason, DecodedAudioPacket, ReceiverInfo, UserInfo} from '../mojom/boca_receiver.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';

export declare interface ClientApi {
  /**
   * Initializes the client with the receiver info.
   */
  onInitReceiverInfo(receiverInfo: ReceiverInfo): void;

  /**
   * Notifies the client that an error has occurred during initialization.
   */
  onInitReceiverError(): void;

  /**
   * Notifies the client that a frame has been received. Calling this method
   * indicates that the receiver is connected to a host. Should be called only
   * after onConnecting is called.
   */
  onFrameReceived(image: BitmapN32): void;

  /**
   * Notifies the client that an audio packet has been received.
   * @param packet The audio packet to be processed.
   */
  onAudioPacket(packet: DecodedAudioPacket): void;

  /**
   * Notifies the client that the receiver is connecting to a new host.
   */
  onConnecting(initiator: UserInfo, presenter: UserInfo|null): void;

  /**
   * Notifies the client that the current connection has been closed.
   */
  onConnectionClosed(reason: ConnectionClosedReason): void;
}

/**
 * Returns the boca app if it can be found in the DOM.
 */
function getApp(): ClientApi {
  const app = document.querySelector('boca-receiver-app')!;
  return app as unknown as ClientApi;
}

/**
 * Runs any initialization code on the boca app once it is in the dom.
 */
function initializeApp(app: ClientApi) {
  const proxy = BrowserProxyImpl.getInstance();

  proxy.callbackRouter.onInitReceiverInfo.addListener(
      (receiverInfo: ReceiverInfo) => app.onInitReceiverInfo(receiverInfo));
  proxy.callbackRouter.onInitReceiverError.addListener(
      () => app.onInitReceiverError());
  proxy.callbackRouter.onFrameReceived.addListener(
      (frameData: BitmapN32) => app.onFrameReceived(frameData));
  proxy.callbackRouter.onAudioPacket.addListener(
      (audioPacket: DecodedAudioPacket) => app.onAudioPacket(audioPacket));
  proxy.callbackRouter.onConnecting.addListener(
      (initiator: UserInfo, presenter: UserInfo|null) =>
          app.onConnecting(initiator, presenter));
  proxy.callbackRouter.onConnectionClosed.addListener(
      (reason: ConnectionClosedReason) => app.onConnectionClosed(reason));
}

/**
 * Called when a mutation occurs on document.body to check if the boca app is
 * available.
 */
function mutationCallback(
    _mutationsList: MutationRecord[], observer: MutationObserver) {
  const app = getApp();
  if (!app) {
    return;
  }
  // The boca app now exists so we can initialize it.
  initializeApp(app);
  observer.disconnect();
}

window.addEventListener('DOMContentLoaded', () => {
  const app = getApp();
  if (app) {
    initializeApp(app);
    return;
  }
  // If translations need to be fetched, the app element may not be added yet.
  // In that case, observe <body> until it is.
  const observer = new MutationObserver(mutationCallback);
  observer.observe(document.body, {childList: true});
});
