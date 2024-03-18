// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the message type used for external party communicating
 * with extension.
 */

import {RequestType, ResponseType} from './message_type.js';
import {Desk, DeskApi, LaunchOptions, RemoveDeskOperands, SwitchDeskOperands, WindowProperties} from './types.js';


/**
 * Request sent to Desk API Bridge extension.
 */
export interface DeskApiBridgeRequest {
  messageType: RequestType;
  operands?: LaunchOptions|string|RemoveDeskOperands|WindowProperties|
      SwitchDeskOperands;
}

/**
 * Response sent from Desk API Bridge extension.
 */
export interface DeskApiBridgeResponse {
  messageType: ResponseType;
  operands: string|Desk|Desk[];
  errorMessage?: string;
}

/**
 * The message sender to the extension.
 */
export interface MessageSender {
  tab: Tab;
}

/**
 * The browser tab which initialize the connection.
 */
export interface Tab {
  windowId: number;
}

/**
 * This interface defines the ServiceWorker that will be used to communicate
 * with the DeskApi
 */
export interface ServiceWorker {
  //@ts-ignore
  registerEventsListener(port: chrome.runtime.Port): void;
  handleMessage(message: DeskApiBridgeRequest, sender: MessageSender):
      Promise<DeskApiBridgeResponse>;
  deskApi: DeskApi;
  isTest: boolean;
}

/**
 * This is the callback function for responding to a received message when
 * either the popup or background scripts receive a message.
 */
export type SendMessageResponse = (response: DeskApiBridgeResponse) => void;

/**
 * Type of chrome.
 */
export type ChromeApi = typeof chrome;
