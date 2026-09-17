// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {enumFromClient} from '../../enum_conversions.js';
import {ExperimentalTriggeringClientReceiver, SubscriberObservationType as SubscriberObservationTypeMojo} from '../../glic.mojom-webui.js';
import type {                                            //
             ExperimentalTriggeringClientInterface,      //
             ExperimentalTriggeringUpdatesHandlerRemote, //
             Screenshot as ScreenshotMojo,               //
             WebClientHandlerRemote,                     //
} from '../../glic.mojom-webui.js';
import type {                                               //
             ExperimentalTriggeringUpdate,                  //
             ExperimentalTriggeringUploadScreenshotRequest, //
             GlicExperimentalTriggeringBrowserHost,         //
             GlicWebClient as WebClient,                    //
             Observable,                                    //
             Screenshot,                                    //
             Subscriber,                                    //
} from '../../glic_api/glic_api.js';
import {Subject} from '../../observable.js';
import {getGuestLoadTimeData} from '../guest_load_time_data.js';
import {optionalFromClient, screenshotToClient} from '../host/conversions.js';
import {ResponseExtras} from '../transport/messaging.js';

export class GlicBrowserHostExperimentalTriggering implements
    ExperimentalTriggeringClientInterface,
    GlicExperimentalTriggeringBrowserHost {
  private readonly uploadEncryptedScreenshotRequestsSubject =
      new Subject<ExperimentalTriggeringUploadScreenshotRequest>();
  private webClient?: WebClient;
  private clientReceiver?: ExperimentalTriggeringClientReceiver;
  private enableStructuredYieldMetadata: boolean|null = null;
  private readonly updateHandlers =
      new Map<ExperimentalTriggeringUpdatesHandlerRemote, Subscriber>();

  initialize(webClient: WebClient, handler: WebClientHandlerRemote) {
    this.webClient = webClient;
    this.clientReceiver?.$.close();
    this.clientReceiver = new ExperimentalTriggeringClientReceiver(this);
    handler.createExperimentalTriggeringClient(
        this.clientReceiver.$.bindNewPipeAndPassRemote());
  }

  destroy(): void {
    if (this.clientReceiver) {
      this.clientReceiver.$.close();
      this.clientReceiver = undefined;
    }
    for (const [handler, subscriber] of this.updateHandlers) {
      handler.$.close();
      subscriber.unsubscribe();
    }
    this.updateHandlers.clear();
  }

  async getExperimentalTriggeringUpdates(
      handler: ExperimentalTriggeringUpdatesHandlerRemote):
      Promise<{success: boolean}> {
    if (!this.webClient) {
      return {success: false};
    }
    const getUpdates = this.webClient.getExperimentalTriggeringUpdates;
    if (!getUpdates) {
      return {success: false};
    }
    const observable = await getUpdates.call(this.webClient);
    if (!observable) {
      return {success: false};
    }

    let completed = false;
    let subscriber: Subscriber|undefined;
    const cleanup = () => {
      if (completed) {
        return;
      }
      completed = true;
      this.updateHandlers.delete(handler);
      subscriber?.unsubscribe();
      subscriber = undefined;
    };

    handler.onConnectionError.addListener(() => {
      cleanup();
    });

    if (this.enableStructuredYieldMetadata === null) {
      this.enableStructuredYieldMetadata =
          getGuestLoadTimeData().enableStructuredYieldMetadata ?? false;
    }
    subscriber = observable.subscribeObserver({
      next: (update: ExperimentalTriggeringUpdate) => {
        handler.onUpdate(
            update ? {
              type: enumFromClient(update.type),
              data: update.data,
              metadata: this.enableStructuredYieldMetadata ?
                  optionalFromClient(update.metadata) :
                  null,
            } :
                     null,
            SubscriberObservationTypeMojo.kUpdate);
      },
      complete: () => {
        handler.onUpdate(null, SubscriberObservationTypeMojo.kComplete);
        cleanup();
      },
      error: (_err: unknown) => {
        handler.onUpdate(null, SubscriberObservationTypeMojo.kError);
        cleanup();
      },
    });

    if (!completed) {
      this.updateHandlers.set(handler, subscriber);
    }
    return {success: true};
  }

  uploadEncryptedScreenshotRequests():
      Observable<ExperimentalTriggeringUploadScreenshotRequest> {
    return this.uploadEncryptedScreenshotRequestsSubject;
  }

  async uploadEncryptedScreenshot(screenshot: ScreenshotMojo):
      Promise<{fileToken: string | null}> {
    const extras = new ResponseExtras();
    const clientScreenshot = screenshotToClient(screenshot, extras);
    if (!clientScreenshot) {
      return {fileToken: null};
    }
    try {
      const fileToken =
          await this.uploadEncryptedScreenshotInternal(clientScreenshot);
      return {fileToken};
    } catch (e) {
      console.warn('Failed to upload encrypted screenshot:', e);
      return {fileToken: null};
    }
  }

  private async uploadEncryptedScreenshotInternal(screenshot: Screenshot):
      Promise<string|null> {
    if (!this.uploadEncryptedScreenshotRequestsSubject
             .hasActiveSubscription()) {
      return null;
    }
    const {promise, resolve, reject} = Promise.withResolvers<string|null>();
    try {
      this.uploadEncryptedScreenshotRequestsSubject.next({
        screenshot,
        uploadComplete: (fileToken: string|null) => {
          resolve(fileToken);
        },
      });
    } catch (e) {
      reject(e);
    }
    return promise;
  }
}
