// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The background script for the desk_api_bridge
 * This module routes Desk API calls to the service worker.
 */

import {ChromeApi, DeskApiBridgeRequest, MessageSender, SendMessageResponse} from './desk_api_types.js';
import {ResponseType} from './message_type.js';
import {ServiceWorkerFactory} from './service_worker.js';
import {DeskApi, NotificationApi} from './types.js';

/**
 * Extension entry point.
 */
export class Background {
  constructor(
      private readonly chromeAPI: ChromeApi, private readonly deskAPI: DeskApi,
      private readonly notifAPI: NotificationApi) {
    const serviceWorkerFactory = new ServiceWorkerFactory(
        this.deskAPI, this.notifAPI, /*isTest=*/ false);
    this.chromeAPI.runtime.onMessageExternal.addListener(
        (request: DeskApiBridgeRequest, sender: unknown,
         sendResponse: SendMessageResponse) => {
          serviceWorkerFactory.get()
              .then((serviceWorker) => {
                serviceWorker.handleMessage(request, sender as MessageSender)
                    .then((response) => {
                      sendResponse(response);
                    })
                    .catch((error: Error) => {
                      sendResponse({
                        messageType: ResponseType.OPERATION_FAILURE,
                        operands: [],
                        errorMessage: error.message,
                      });
                    });
              })
              .catch((error: Error) => {
                sendResponse({
                  messageType: ResponseType.OPERATION_FAILURE,
                  operands: [],
                  errorMessage: error.message,
                });
              });

          // assures that the service worker will continue to accept requests
          // after it's no longer active.
          return true;
        });


    // @ts-ignore: onConnectExternal defined in chrome.runtime
    this.chromeAPI.runtime.onConnectExternal.addListener(
        // @ts-ignore: Port defined in chrome.runtime
        (port: chrome.runtime.Port) => {
          serviceWorkerFactory.get().then(serviceWorker => {
            serviceWorker.registerEventsListener(port);
          });
        });
  }
}
