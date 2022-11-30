// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The background script for the desk_api_bridge
 * This module routes Desk API calls to the service worker.
 */

import {DeskApiImpl} from './desk_api_impl';
import {DeskApiBridgeRequest, MessageSender, SendMessageResponse} from './desk_api_types';
import {ResponseType} from './message_type';
import {NotificationApiImpl} from './notification_api_impl';
import {ServiceWorkerFactory} from './service_worker';


chrome.runtime.onMessageExternal.addListener(
    (request: DeskApiBridgeRequest, sender: unknown,
     sendResponse: SendMessageResponse) => {
      const serviceWorkerFactory = new ServiceWorkerFactory(
          new DeskApiImpl(), new NotificationApiImpl(), /*isTest=*/ false);
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

      // assures that the service worker will continue to accept requests after
      // it's no longer active.
      return true;
    });
