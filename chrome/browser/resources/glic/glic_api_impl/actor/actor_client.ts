// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Web client side actor message handler.

import type {ActorTaskState, GlicBrowserHostJournal, Journal, NavigationConfirmationRequest, SelectAutofillSuggestionsDialogRequest, SelectCredentialDialogRequest, UserConfirmationDialogRequest} from '../../glic_api/glic_api.js';
import type {GlicBrowserHostImpl} from '../client/glic_api_client.js';
import {rgbaImageToBlob} from '../client/image_utils.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';
import type {PostMessageRemote} from '../transport/post_message_transport.js';

import {ConfirmationRequestErrorReason, SelectAutofillSuggestionsDialogErrorReason, SelectCredentialDialogErrorReason} from './actor_types.js';
import type {ActorClient, ActorHost, CredentialPrivate, NavigationConfirmationRequestPrivate, NavigationConfirmationResponsePrivate, SelectAutofillSuggestionsDialogRequestPrivate, SelectAutofillSuggestionsDialogResponsePrivate, SelectCredentialDialogRequestPrivate, SelectCredentialDialogResponsePrivate, UserConfirmationDialogRequestPrivate, UserConfirmationDialogResponsePrivate} from './actor_types.js';

export class ActorWebClientMessageHandler implements
    MessageHandlerInterface<ActorClient> {
  constructor(private host: GlicBrowserHostImpl) {}

  glicWebClientNotifyActorTaskStateChanged(
      payload: {taskId: number, state: ActorTaskState}): void {
    this.host.setActorTaskState(payload.taskId, payload.state);
  }

  async glicWebClientRequestToShowDialog(payload: {
    request: SelectCredentialDialogRequestPrivate,
  }): Promise<{response: SelectCredentialDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.host.selectCredentialDialogRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if no credential is selected.
        window.console.warn(
            'GlicWebClient: no subscriber for' +
            ' selectCredentialDialogRequest()!');
        resolve({
          response: {
            taskId: request.taskId,
            errorReason:
                SelectCredentialDialogErrorReason.DIALOG_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const iconsGetter = new Map<string, () => Promise<Blob>>();
      for (const [id, image] of payload.request.icons.entries()) {
        let promise: Promise<Blob>|undefined;
        iconsGetter.set(id, () => {
          if (!promise) {
            promise = Promise.resolve(rgbaImageToBlob(image));
          }
          return promise;
        });
      }
      const credentials =
          request.credentials.map((credential: CredentialPrivate) => {
            const getIcon = iconsGetter.get(credential.sourceSiteOrApp);
            const accountPicture = credential.accountPicture;
            const getAccountPicture = accountPicture ?
                () => Promise.resolve(rgbaImageToBlob(accountPicture)) :
                undefined;
            return {
              ...credential,
              getIcon,
              getAccountPicture,
            };
          });
      const requestWithCallback: SelectCredentialDialogRequest = {
        ...request,
        credentials,
        onDialogClosed: resolve,
      };
      this.host.selectCredentialDialogRequestSubject.next(requestWithCallback);
    });
  }

  glicWebClientRequestToShowConfirmationDialog(payload: {
    request: UserConfirmationDialogRequestPrivate,
  }): Promise<{response: UserConfirmationDialogResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.host.userConfirmationDialogRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if the user denied the request.
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'userConfirmationDialogRequest()!');
        resolve({
          response: {
            permissionGranted: false,
            errorReason:
                ConfirmationRequestErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: UserConfirmationDialogRequest = {
        ...payload.request,
        onDialogClosed: resolve,
      };
      this.host.userConfirmationDialogRequestSubject.next(requestWithCallback);
    });
  }

  glicWebClientRequestToConfirmNavigation(payload: {
    request: NavigationConfirmationRequestPrivate,
  }): Promise<{response: NavigationConfirmationResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.host.navigationConfirmationRequestSubject
               .hasActiveSubscription()) {
        // Since there is no subscriber, respond to the browser immediately as
        // if the user denied the request.
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'navigationConfirmationRequest()!');
        resolve({
          response: {
            errorReason:
                ConfirmationRequestErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: NavigationConfirmationRequest = {
        ...payload.request,
        onConfirmationDecision: resolve,
      };
      this.host.navigationConfirmationRequestSubject.next(requestWithCallback);
    });
  }

  async glicWebClientRequestToShowAutofillSuggestionsDialog(payload: {
    request: SelectAutofillSuggestionsDialogRequestPrivate,
  }): Promise<{response: SelectAutofillSuggestionsDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.host.selectAutofillSuggestionsDialogRequestSubject
               .hasActiveSubscription()) {
        resolve({
          response: {
            taskId: request.taskId,
            errorReason: SelectAutofillSuggestionsDialogErrorReason
                             .DIALOG_PROMISE_NO_SUBSCRIBER,
            selectedSuggestions: [],
          },
        });
        return;
      }
      const requestWithCallback: SelectAutofillSuggestionsDialogRequest = {
        ...request,
        formFillingRequests: request.formFillingRequests.map(
            formFillingRequest => ({
              ...formFillingRequest,
              suggestions: formFillingRequest.suggestions.map(suggestion => {
                const icon = suggestion.icon;
                const getIcon = icon ?
                    () => Promise.resolve(rgbaImageToBlob(icon)) :
                    undefined;
                return {...suggestion, getIcon};
              }),
            })),
        onDialogClosed: (result) => {
          const response: SelectAutofillSuggestionsDialogResponsePrivate = {
            ...result.response,
            taskId: request.taskId,
          };
          resolve({
            response: response,
          });
        },
        onFormPresented: (params) => {
          this.host.autofillSuggestionDialogOnFormPresented(
              request.taskId, params);
        },
        onFormPreviewChanged: (params) => {
          this.host.autofillSuggestionDialogOnFormPreviewChanged(
              request.taskId, params);
        },
        onFormConfirmed: (params) => {
          this.host.autofillSuggestionDialogOnFormConfirmed(
              request.taskId, params);
        },
      };
      this.host.selectAutofillSuggestionsDialogRequestSubject.next(
          requestWithCallback);
    });
  }
}


export class GlicBrowserHostJournalImpl implements GlicBrowserHostJournal {
  constructor(public sender: PostMessageRemote<ActorHost>) {}

  beginAsyncEvent(
      asyncEventId: number, taskId: number, event: string,
      details: string): void {
    this.sender.requestNoResponse(
        'logBeginAsyncEvent', {asyncEventId, taskId, event, details});
  }

  clear(): void {
    this.sender.requestNoResponse('journalClear', undefined);
  }

  endAsyncEvent(asyncEventId: number, details: string): void {
    this.sender.requestNoResponse('logEndAsyncEvent', {asyncEventId, details});
  }

  instantEvent(taskId: number, event: string, details: string): void {
    this.sender.requestNoResponse('logInstantEvent', {taskId, event, details});
  }

  async snapshot(clear: boolean): Promise<Journal> {
    const snapshotResult =
        await this.sender.requestWithResponse('journalSnapshot', {clear});
    return snapshotResult.journal;
  }

  start(maxBytes: number, captureScreenshots: boolean): void {
    this.sender.requestNoResponse(
        'journalStart', {maxBytes, captureScreenshots});
  }

  stop(): void {
    this.sender.requestNoResponse('journalStop', undefined);
  }

  recordFeedback(positive: boolean, reason: string) {
    this.sender.requestNoResponse(
        'journalRecordFeedback',
        {positive, reason},
    );
  }
}
