// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExperimentalTriggeringUpdate, ExperimentalTriggeringUploadScreenshotRequest, GlicExperimentalTriggeringBrowserHost, GlicWebClient as WebClient, Observable, Screenshot} from '../../glic_api/glic_api.js';
import {Subject} from '../../observable.js';
import {SubscriberObservationType} from '../request_types.js';
import type {WebClientHost} from '../request_types.js';
import type {PendingReceiver, PostMessageHandler, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {ExperimentalTriggeringClientDef} from './experimental_triggering_types.js';
import type {ExperimentalTriggeringClient} from './experimental_triggering_types.js';

class ExperimentalTriggeringWebClientMessageHandler implements
    PostMessageHandler<ExperimentalTriggeringClient> {
  constructor(private host: GlicBrowserHostExperimentalTriggering) {}

  async uploadEncryptedScreenshot(payload: {screenshot: Screenshot}):
      Promise<{fileToken: string | null}> {
    return {
      fileToken: await this.host.uploadEncryptedScreenshot(payload.screenshot),
    };
  }

  async getExperimentalTriggeringUpdates(payload: {observationId: number}):
      Promise<{success: boolean}> {
    return this.host.getExperimentalTriggeringUpdates(payload);
  }
}

export class GlicBrowserHostExperimentalTriggering implements
    GlicExperimentalTriggeringBrowserHost {
  private readonly uploadEncryptedScreenshotRequestsSubject =
      new Subject<ExperimentalTriggeringUploadScreenshotRequest>();
  private webClient?: WebClient;
  private clientRemote?: PostMessageRemote<WebClientHost>;

  initialize(
      router: PostMessageRouter,
      receiver: PendingReceiver<ExperimentalTriggeringClient>|undefined,
      webClient: WebClient, clientRemote: PostMessageRemote<WebClientHost>) {
    this.webClient = webClient;
    this.clientRemote = clientRemote;
    if (!receiver) {
      return;
    }
    router.newReceiver(
        receiver, new ExperimentalTriggeringWebClientMessageHandler(this),
        ExperimentalTriggeringClientDef);
  }

  async getExperimentalTriggeringUpdates(payload: {observationId: number}):
      Promise<{success: boolean}> {
    if (!this.webClient || !this.clientRemote) {
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
    const subscriber = observable.subscribeObserver({
      next: (update: ExperimentalTriggeringUpdate) => {
        this.clientRemote!.requestNoResponse('onExperimentalTriggeringUpdate', {
          observationId: payload.observationId,
          update,
          observation: SubscriberObservationType.UPDATE,
        });
      },
      complete: () => {
        this.clientRemote!.requestNoResponse('onExperimentalTriggeringUpdate', {
          observationId: payload.observationId,
          observation: SubscriberObservationType.COMPLETE,
        });
        if (subscriber) {
          subscriber.unsubscribe();
        }
      },
      error: (_err: unknown) => {
        this.clientRemote!.requestNoResponse('onExperimentalTriggeringUpdate', {
          observationId: payload.observationId,
          observation: SubscriberObservationType.ERROR,
        });
        if (subscriber) {
          subscriber.unsubscribe();
        }
      },
    });
    return {success: true};
  }

  uploadEncryptedScreenshotRequests():
      Observable<ExperimentalTriggeringUploadScreenshotRequest> {
    return this.uploadEncryptedScreenshotRequestsSubject;
  }

  async uploadEncryptedScreenshot(screenshot: Screenshot):
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
