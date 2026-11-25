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
import type {                                                   //
             ExperimentalTriggeringConfirmationResponseRequest, //
             ExperimentalTriggeringUpdate,                      //
             ExperimentalTriggeringUploadScreenshotRequest,     //
             GlicExperimentalTriggeringBrowserHost,             //
             GlicWebClient as WebClient,                        //
             Observable,                                        //
             Screenshot,                                        //
             Subscriber,                                        //
} from '../../glic_api/glic_api.js';
import {Subject} from '../../observable.js';
import {optionalFromClient, screenshotToClient} from '../conversions.js';
import {getGuestLoadTimeData} from '../guest_load_time_data.js';

// How long to wait for the web client to report whether a confirmation
// response was applied. Applying a response is a local operation, so the
// client is expected to reply promptly. If it doesn't, the response is
// reported as not applied so that the browser's reply callback is never left
// pending indefinitely.
const CONFIRMATION_RESPONSE_TIMEOUT_MS = 30 * 1000;

export class GlicBrowserHostExperimentalTriggering implements
    ExperimentalTriggeringClientInterface,
    GlicExperimentalTriggeringBrowserHost {
  // TODO(csharrison): Apply the same timeout handling used for
  // `confirmationResponsesSubject` here, so that an upload which never
  // completes can't leave the browser's reply callback pending.
  private readonly uploadEncryptedScreenshotRequestsSubject =
      new Subject<ExperimentalTriggeringUploadScreenshotRequest>();
  private readonly confirmationResponsesSubject =
      new Subject<ExperimentalTriggeringConfirmationResponseRequest>();
  private webClient?: WebClient;
  private clientReceiver?: ExperimentalTriggeringClientReceiver;
  private enableStructuredYieldMetadata: boolean|null = null;

  initialize(webClient: WebClient, handler: WebClientHandlerRemote) {
    this.webClient = webClient;
    this.clientReceiver?.$.close();
    this.clientReceiver = new ExperimentalTriggeringClientReceiver(this);
    handler.createExperimentalTriggeringClient(
        this.clientReceiver.$.bindNewPipeAndPassRemote());
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

    return {success: true};
  }

  uploadEncryptedScreenshotRequests():
      Observable<ExperimentalTriggeringUploadScreenshotRequest> {
    return this.uploadEncryptedScreenshotRequestsSubject;
  }

  confirmationResponses():
      Observable<ExperimentalTriggeringConfirmationResponseRequest> {
    return this.confirmationResponsesSubject;
  }

  async submitExperimentalTriggeringConfirmationResponse(response: number[]):
      Promise<{accepted: boolean}> {
    if (!this.confirmationResponsesSubject.hasActiveSubscription()) {
      return {accepted: false};
    }
    const buffer = new Uint8Array(response).buffer;
    const {promise, resolve} = Promise.withResolvers<boolean>();
    // Resolves the reply on whichever happens first: the client calling
    // onComplete() or the timeout below. Later calls are no-ops, so the client
    // cannot resolve the same request twice.
    let settled = false;
    const settle = (applied: boolean) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeoutId);
      resolve(applied);
    };
    // Only ever read from `settle`, which cannot run until after the request
    // is emitted below.
    const timeoutId = setTimeout(() => {
      console.warn('Timed out waiting for a confirmation response result.');
      settle(false);
    }, CONFIRMATION_RESPONSE_TIMEOUT_MS);
    try {
      this.confirmationResponsesSubject.next({
        response: buffer,
        onComplete: (applied: boolean) => {
          settle(applied);
        },
      });
    } catch (e) {
      console.warn('Failed to deliver confirmation response:', e);
      settle(false);
    }
    return {accepted: await promise};
  }

  async uploadEncryptedScreenshot(screenshot: ScreenshotMojo):
      Promise<{fileToken: string | null}> {
    const clientScreenshot = screenshotToClient(screenshot);
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
