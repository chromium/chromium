// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides the implementation for the service worker
 * class as declared in types.d.ts
 */

import {DeskApiBridgeRequest, DeskApiBridgeResponse, MessageSender, ServiceWorker} from './desk_api_types.js';
import {EventType, RequestType, ResponseType} from './message_type.js';
import {Desk, DeskApi, GetDeskByIdOperands, LaunchOptions, NotificationApi, NotificationOptions, RemoveDeskOperands, SwitchDeskOperands, WindowProperties} from './types.js';


/**
 * Actual implementation for the service worker.
 */
class ServiceWorkerImpl implements ServiceWorker {
  /**
   * This constructor sets three fields: the deskApi and notification field
   * exists in order to facilitate dependency injection for this class, the
   * isTest boolean field exists in order to short circuit reads to
   * chrome.runtime.LastError during tests.  This is done because said variable
   * is not available in the test environment and there is no way to inject a
   * reference to that error here.
   */
  constructor(
      public deskApi: DeskApi, public notificationApi: NotificationApi,
      public isTest: boolean) {}

  launchDeskPromise(options: LaunchOptions) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.launchDesk(options, (deskId: string) => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({
            messageType: ResponseType.OPERATION_SUCCESS,
            operands: {'deskUuid': deskId},
          });
        });
      } catch (error: unknown) {
        reject(error);
      }
    });
  }

  removeDeskPromise(operands: RemoveDeskOperands) {
    if (!operands.deskId) {
      return Promise.reject({message: 'The desk can not be found.'});
    }

    if (operands.options) {
      if (operands.options.allowUndo && operands.options.combineDesks) {
        return Promise.reject({
          message:
              'Unsupported behavior: allowUndo and combineDesks are both true.',
        });
      }
    }

    if (operands.skipConfirmation) {
      return this.removeDeskInternalPromise(operands);
    }

    const notificationId = 'deskAPI';
    this.notificationApi.clear(notificationId);
    const defaultNotificationOptions: NotificationOptions = {
      title: 'Close all windows?',
      message: 'This interaction has ended. ' +
          'This will close all windows on this desk.',
      iconUrl: 'templates_icon.png',
      buttons: [{title: 'Close windows'}, {title: 'Keep windows'}],
    };
    const notificationOptions =
        !operands.confirmationSetting ? defaultNotificationOptions : {
          title: operands.confirmationSetting.title,
          message: operands.confirmationSetting.message,
          iconUrl: operands.confirmationSetting.iconUrl,
          buttons: [
            {title: operands.confirmationSetting.acceptMessage},
            {title: operands.confirmationSetting.rejectMessage},
          ],
        };

    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      // First, check if the desk is able to be obtained.
      this.getDeskByIdPromise({deskId: operands.deskId})
          .then(() => {
            resolve(this.removeDeskNotificationPromise(
                operands, notificationOptions, notificationId));
          })
          .catch(error => {
            reject(error);
          });
    });
  }

  removeDeskNotificationPromise(
      operands: RemoveDeskOperands, notificationOptions: NotificationOptions,
      notificationId: string) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        //  Create a pop up window
        //  Set notificationId so that the same notification needs to be
        //  cancelled before the new notification can be created
        this.notificationApi.create(notificationId, notificationOptions, () => {
          resolve(this.removeDeskAddNotificationListenerPromise(operands));
        });
      } catch (error: unknown) {
        this.notificationApi.clear(notificationId);
        reject(error);
      }
    });
  }

  removeDeskAddNotificationListenerPromise(operands: RemoveDeskOperands) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      this.notificationApi.addClickEventListener(
          (notificationId, buttonIndex) => {
            if (buttonIndex === 0) {
              this.removeDeskInternalPromise(operands)
                  .then((result) => {
                    resolve(result);
                  })
                  .catch(error => {
                    reject(error);
                  });
            } else {
              reject(new Error('User cancelled desk removal operation'));
            }
            this.notificationApi.clear(notificationId);
          });
    });
  }

  removeDeskInternalPromise(operands: RemoveDeskOperands) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.removeDesk(operands.deskId, operands.options, () => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({
            messageType: ResponseType.OPERATION_SUCCESS,
            operands: [],
          });
        });
      } catch (error: unknown) {
        reject(error);
      }
    });
  }

  setWindowPropertiesPromise(
      operands: WindowProperties, sender: MessageSender) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.setWindowProperties(sender.tab.windowId, operands, () => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({messageType: ResponseType.OPERATION_SUCCESS, operands: []});
        });

      } catch (error: unknown) {
        reject(error);
      }
    });
  }

  getActiveDeskPromise() {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.getActiveDesk((deskId: string) => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({
            messageType: ResponseType.OPERATION_SUCCESS,
            operands: {'deskUuid': deskId},
          });
        });

      } catch (error: unknown) {
        reject(error);
      }
    });
  }

  switchDeskPromise(operands: SwitchDeskOperands) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.switchDesk(operands.deskId, () => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({messageType: ResponseType.OPERATION_SUCCESS, operands: []});
        });

      } catch (error: unknown) {
        reject(error);
      }
    });
  }

  getDeskByIdPromise(operands: GetDeskByIdOperands) {
    return new Promise<DeskApiBridgeResponse>((resolve, reject) => {
      try {
        this.deskApi.getDeskById(operands.deskId, (desk: Desk) => {
          if (!this.isTest && chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          resolve({
            messageType: ResponseType.OPERATION_SUCCESS,
            operands: {'deskUuid': desk.deskUuid, 'deskName': desk.deskName},
          });
        });
      } catch (error: unknown) {
        reject(error);
      }
    });
  }
  /**
   * This function handles a message and returns a promise containing
   * the result of the operation called for by the RequestType field.
   */
  handleMessage(message: DeskApiBridgeRequest, sender: MessageSender):
      Promise<DeskApiBridgeResponse> {
    switch (message.messageType) {
      case RequestType.LAUNCH_DESK:
        return this.launchDeskPromise(message.operands as LaunchOptions);
      case RequestType.REMOVE_DESK:
        return this.removeDeskPromise(message.operands as RemoveDeskOperands);
      case RequestType.SET_WINDOW_PROPERTIES:
        return this.setWindowPropertiesPromise(
            message.operands as WindowProperties, sender);
      case RequestType.GET_ACTIVE_DESK:
        return this.getActiveDeskPromise();
      case RequestType.SWITCH_DESK:
        return this.switchDeskPromise(message.operands as SwitchDeskOperands);
      case RequestType.GET_DESK_BY_ID:
        return this.getDeskByIdPromise(message.operands as GetDeskByIdOperands);
      default:
        throw new Error(`message of unknown type: ${message.messageType}!`);
    }
  }

  /**
   * This function register listener for desk events.
   * @param port port for communicating with web page
   */
  //@ts-ignore
  registerEventsListener(port: chrome.runtime.Port): void {
    this.deskApi.addDeskAddedListener((id: string, fromUndo = false) => {
      const eventType = fromUndo ? EventType.DESK_UNDONE : EventType.DESK_ADDED;
      port.postMessage({'eventName': eventType, 'data': {'deskId': id}});
    });
    this.deskApi.addDeskRemovedListener((id: string) => {
      port.postMessage(
          {'eventName': EventType.DESK_REMOVED, 'data': {'deskId': id}});
    });
    this.deskApi.addDeskSwitchedListener((a: string, d: string) => {
      port.postMessage({
        'eventName': EventType.DESK_SWITCHED,
        'data': {'activated': a, 'deactivated': d},
      });
    });
  }
}

/**
 * This class manages the single service worker instance and is the point of
 * injections for its dependencies.
 */
export class ServiceWorkerFactory {
  // global managed instance.
  private static serviceWorkerInstance: ServiceWorker;

  constructor(
      deskApi: DeskApi, notificationApi: NotificationApi, isTest: boolean) {
    ServiceWorkerFactory.serviceWorkerInstance =
        new ServiceWorkerImpl(deskApi, notificationApi, isTest);
  }

  get(): Promise<ServiceWorker> {
    return new Promise<ServiceWorker>((resolve, reject) => {
      if (ServiceWorkerFactory.serviceWorkerInstance === null) {
        reject(new Error('Service worker factory not constructed!'));
        return;
      }

      resolve(ServiceWorkerFactory.serviceWorkerInstance);
    });
  }
}
