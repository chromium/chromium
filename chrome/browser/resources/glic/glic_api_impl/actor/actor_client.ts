// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Web client side actor message handler.

import {assert} from '//resources/js/assert.js';

import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import type {ActorTaskInterruptReason, CancelActionsResult, CreateActorTabOptions, FormFillingResponse, GlicBrowserHost, GlicBrowserHostJournal, GmailOtpConfirmationRequest, GmailOtpOptInRequest, Journal, NavigationConfirmationRequest, Observable, ObservableValue, ResumeActorTaskResult, SelectAutofillSuggestionsDialogRequest, SelectCredentialDialogRequest, TabContextOptions, TabContextResult, TabData, TaskOptions, UserConfirmationDialogRequest} from '../../glic_api/glic_api.js';
import {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';
import {convertTabContextResultFromPrivate, convertTabDataFromPrivate} from '../client/glic_api_client.js';
import type {GlicBrowserHostBaseContext} from '../client/glic_client_common.js';
import {rgbaImageToBlob} from '../client/image_utils.js';
import type {PendingReceiver, PendingRemote, PostMessageHandler, PostMessageRemote} from '../transport/post_message_transport.js';

import {ActorClientDef, ConfirmationRequestErrorReason, GmailOtpErrorReason, SelectAutofillSuggestionsDialogErrorReason, SelectCredentialDialogErrorReason} from './actor_types.js';
import type {ActorClient, ActorHost, CredentialPrivate, GmailOtpConfirmationRequestPrivate, GmailOtpConfirmationResponsePrivate, GmailOtpOptInRequestPrivate, GmailOtpOptInResponsePrivate, NavigationConfirmationRequestPrivate, NavigationConfirmationResponsePrivate, SelectAutofillSuggestionsDialogRequestPrivate, SelectAutofillSuggestionsDialogResponsePrivate, SelectCredentialDialogRequestPrivate, SelectCredentialDialogResponsePrivate, UserConfirmationDialogRequestPrivate, UserConfirmationDialogResponsePrivate} from './actor_types.js';

// Implements actor-specific methods on GlicBrowserHost.
export class GlicBrowserHostActor implements Partial<GlicBrowserHost> {
  private actorWebClientMessageHandler: ActorWebClientMessageHandler;
  readonly userConfirmationDialogRequestSubject =
      new Subject<UserConfirmationDialogRequest>();
  readonly selectCredentialDialogRequestSubject =
      new Subject<SelectCredentialDialogRequest>();
  readonly navigationConfirmationRequestSubject =
      new Subject<NavigationConfirmationRequest>();
  private actorSender?: PostMessageRemote<ActorHost>;
  private actorTaskState =
      new Map<number, ObservableValueImpl<ActorTaskState>>();
  readonly selectAutofillSuggestionsDialogRequestSubject =
      new Subject<SelectAutofillSuggestionsDialogRequest>();
  readonly selectGmailOtpOptInRequestSubject =
      new Subject<GmailOtpOptInRequest>();
  readonly selectGmailOtpConfirmationRequestSubject =
      new Subject<GmailOtpConfirmationRequest>();

  private journalHost?: GlicBrowserHostJournalImpl;
  actOnWebCapabilityValue = ObservableValueImpl.withNoValue<boolean>();
  readonly actorTaskListRowClickedSubject = new Subject<number>();
  // TODO: Remove this from the API.
  actInFocusedTab = undefined;

  constructor(private host: GlicBrowserHostBaseContext) {
    this.actorWebClientMessageHandler = new ActorWebClientMessageHandler(this);
  }

  initialize(
      initialState: WebClientInitialState,
      actorRemote: PendingRemote<ActorHost>|undefined,
      actorReceiver: PendingReceiver<ActorClient>|undefined) {
    if (actorRemote === undefined || actorReceiver === undefined ||
        !initialState.enableActInFocusedTab) {
      this.selectUserConfirmationDialogRequestHandler = undefined;
      this.selectCredentialDialogRequestHandler = undefined;
      this.selectNavigationConfirmationRequestHandler = undefined;
      this.selectAutofillSuggestionsDialogRequestHandler = undefined;
      this.getContextForActorFromTab = undefined;
      this.createTask = undefined;
      this.performActions = undefined;
      this.cancelActions = undefined;
      this.stopActorTask = undefined;
      this.pauseActorTask = undefined;
      this.resumeActorTask = undefined;
      this.interruptActorTask = undefined;
      this.uninterruptActorTask = undefined;
      this.updateActorTaskStepProgress = undefined;
      this.getActorTaskState = undefined;
      this.createActorTab = undefined;
      this.getActOnWebCapability = undefined;
      this.actorTaskListRowClicked = undefined;
      this.getJournalHost = undefined;
      return;
    }

    this.actorSender = this.host.router.newRemote(actorRemote);
    this.host.router.newReceiver(
        actorReceiver, this.actorWebClientMessageHandler, ActorClientDef);
    this.journalHost = new GlicBrowserHostJournalImpl(this.actorSender);
    if (!initialState.enableGmailOtpOptIn) {
      this.selectGmailOtpOptInRequestHandler = undefined;
    }
    if (!initialState.enableGmailOtpConfirmation) {
      this.selectGmailOtpConfirmationRequestHandler = undefined;
    }
    if (!initialState.enableGetContextActor) {
      // MOJO_RUNTIME_FEATURE_GATED GetContextForActorFromTab
      this.getContextForActorFromTab = undefined;
    }
  }

  setActorTaskState(taskId: number, state: ActorTaskState): void {
    const stateObs =
        this.getActorTaskState?.(taskId) as ObservableValueImpl<ActorTaskState>|
        undefined;
    stateObs?.assignAndSignal(state);

    if (state === ActorTaskState.STOPPED) {
      this.actorTaskState.delete(taskId);
    }
  }

  selectUserConfirmationDialogRequestHandler?
      (): Observable<UserConfirmationDialogRequest> {
    return this.userConfirmationDialogRequestSubject;
  }

  selectCredentialDialogRequestHandler?
      (): Observable<SelectCredentialDialogRequest> {
    return this.selectCredentialDialogRequestSubject;
  }

  selectNavigationConfirmationRequestHandler?
      (): Observable<NavigationConfirmationRequest> {
    return this.navigationConfirmationRequestSubject;
  }

  selectGmailOtpOptInRequestHandler?(): Observable<GmailOtpOptInRequest> {
    return this.selectGmailOtpOptInRequestSubject;
  }

  selectGmailOtpConfirmationRequestHandler?
      (): Observable<GmailOtpConfirmationRequest> {
    return this.selectGmailOtpConfirmationRequestSubject;
  }

  selectAutofillSuggestionsDialogRequestHandler?
      (): Observable<SelectAutofillSuggestionsDialogRequest> {
    return this.selectAutofillSuggestionsDialogRequestSubject;
  }

  autofillSuggestionDialogOnFormPresented(taskId: number, params: {
    formFillingRequestIndex: number,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormPresented', {taskId, params});
  }

  autofillSuggestionDialogOnFormPreviewChanged(taskId: number, params: {
    formFillingRequestIndex: number,
    response?: FormFillingResponse,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormPreviewChanged', {taskId, params});
  }

  autofillSuggestionDialogOnFormConfirmed(taskId: number, params: {
    formFillingRequestIndex: number,
    response: FormFillingResponse,
  }): void {
    this.actorSender?.requestNoResponse(
        'autofillSuggestionDialogOnFormConfirmed', {taskId, params});
  }

  async getContextForActorFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    assert(this.actorSender);
    const result = await this.actorSender.requestWithResponse(
        'getContextForActorFromTab', {tabId, options});
    return convertTabContextResultFromPrivate(result.tabContextResult);
  }

  async createTask?(taskOptions?: TaskOptions): Promise<number> {
    assert(this.actorSender);
    const result =
        await this.actorSender.requestWithResponse('createTask', {taskOptions});
    return result.taskId;
  }

  async performActions?(actions: ArrayBuffer): Promise<ArrayBuffer> {
    assert(this.actorSender);
    const result =
        await this.actorSender.requestWithResponse('performActions', {actions});
    return result.actionsResult;
  }

  async cancelActions?(taskId: number): Promise<CancelActionsResult> {
    assert(this.actorSender);
    const response =
        await this.actorSender.requestWithResponse('cancelActions', {taskId});
    return response.result;
  }

  stopActorTask?(taskId?: number, stopReason?: ActorTaskStopReason): void {
    this.actorSender?.requestNoResponse('stopActorTask', {
      taskId: taskId ?? 0,
      stopReason: stopReason ?? ActorTaskStopReason.TASK_COMPLETE,
    });
  }

  pauseActorTask?
      (taskId: number, pauseReason?: ActorTaskPauseReason, tabId?: string):
          void {
    this.actorSender?.requestNoResponse('pauseActorTask', {
      taskId,
      pauseReason: pauseReason ?? ActorTaskPauseReason.PAUSED_BY_MODEL,
      tabId: tabId ?? '',
    });
  }

  async resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
      Promise<ResumeActorTaskResult> {
    assert(this.actorSender);
    const response = await this.actorSender.requestWithResponse(
        'resumeActorTask', {taskId, tabContextOptions});
    return convertTabContextResultFromPrivate(response.resumeActorTaskResult);
  }

  interruptActorTask?
      (taskId: number, interruptReason?: ActorTaskInterruptReason): void {
    this.actorSender?.requestNoResponse('interruptActorTask', {
      taskId,
      interruptReason,
    });
  }

  uninterruptActorTask?(taskId: number): void {
    this.actorSender?.requestNoResponse('uninterruptActorTask', {
      taskId,
    });
  }

  updateActorTaskStepProgress?(taskId: number, stepProgress: string): void {
    this.actorSender?.requestNoResponse('updateActorTaskStepProgress', {
      taskId,
      stepProgress,
    });
  }

  getActorTaskState?(taskId: number): ObservableValue<ActorTaskState> {
    const stateObs = this.actorTaskState.get(taskId);
    if (stateObs) {
      return stateObs;
    }
    // TODO(mcnee): The client could pass an id that will never have
    // state updates (e.g. the task already finished and we cleared the old
    // observable in setActorTaskState). Consider removing these cases from the
    // map when all subscribers are removed.
    const newObs = ObservableValueImpl.withNoValue<ActorTaskState>();
    this.actorTaskState.set(taskId, newObs);
    return newObs;
  }

  async createActorTab?
      (taskId: number, options: CreateActorTabOptions): Promise<TabData> {
    assert(this.actorSender);
    const result = await this.actorSender.requestWithResponse(
        'createActorTab', {taskId, options});
    if (!result.tabData) {
      throw new Error('createActorTab: failed');
    }
    return convertTabDataFromPrivate(result.tabData);
  }

  getActOnWebCapability?(): ObservableValue<boolean> {
    return this.actOnWebCapabilityValue;
  }

  actorTaskListRowClicked?(): Observable<number> {
    return this.actorTaskListRowClickedSubject;
  }

  getJournalHost?(): GlicBrowserHostJournal {
    assert(this.journalHost);
    return this.journalHost;
  }
}

// Handles postMessage messages from the host.
export class ActorWebClientMessageHandler implements
    PostMessageHandler<ActorClient> {
  constructor(private actorHost: GlicBrowserHostActor) {}

  notifyActorTaskStateChanged(payload: {taskId: number, state: ActorTaskState}):
      void {
    this.actorHost.setActorTaskState(payload.taskId, payload.state);
  }

  async requestToShowDialog(payload: {
    request: SelectCredentialDialogRequestPrivate,
  }): Promise<{response: SelectCredentialDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.actorHost.selectCredentialDialogRequestSubject
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
      this.actorHost.selectCredentialDialogRequestSubject.next(
          requestWithCallback);
    });
  }

  requestToShowConfirmationDialog(payload: {
    request: UserConfirmationDialogRequestPrivate,
  }): Promise<{response: UserConfirmationDialogResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.actorHost.userConfirmationDialogRequestSubject
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
      this.actorHost.userConfirmationDialogRequestSubject.next(
          requestWithCallback);
    });
  }

  requestToConfirmNavigation(payload: {
    request: NavigationConfirmationRequestPrivate,
  }): Promise<{response: NavigationConfirmationResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.actorHost.navigationConfirmationRequestSubject
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
      this.actorHost.navigationConfirmationRequestSubject.next(
          requestWithCallback);
    });
  }

  async requestToShowAutofillSuggestionsDialog(payload: {
    request: SelectAutofillSuggestionsDialogRequestPrivate,
  }): Promise<{response: SelectAutofillSuggestionsDialogResponsePrivate}> {
    const request = payload.request;
    return new Promise(resolve => {
      if (!this.actorHost.selectAutofillSuggestionsDialogRequestSubject
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
          this.actorHost.autofillSuggestionDialogOnFormPresented(
              request.taskId, params);
        },
        onFormPreviewChanged: (params) => {
          this.actorHost.autofillSuggestionDialogOnFormPreviewChanged(
              request.taskId, params);
        },
        onFormConfirmed: (params) => {
          this.actorHost.autofillSuggestionDialogOnFormConfirmed(
              request.taskId, params);
        },
      };
      this.actorHost.selectAutofillSuggestionsDialogRequestSubject.next(
          requestWithCallback);
    });
  }

  requestToShowGmailOtpOptInDialog(payload: {
    request: GmailOtpOptInRequestPrivate,
  }): Promise<{response: GmailOtpOptInResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.actorHost.selectGmailOtpOptInRequestSubject
               .hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'selectGmailOtpOptInRequestHandler()!');
        resolve({
          response: {
            permissionGranted: false,
            errorReason: GmailOtpErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: GmailOtpOptInRequest = {
        ...payload.request,
        onDialogClosed: (response) => {
          resolve({response});
        },
      };
      this.actorHost.selectGmailOtpOptInRequestSubject.next(requestWithCallback);
    });
  }

  requestToShowGmailOtpConfirmationDialog(payload: {
    request: GmailOtpConfirmationRequestPrivate,
  }): Promise<{response: GmailOtpConfirmationResponsePrivate}> {
    return new Promise(resolve => {
      if (!this.actorHost.selectGmailOtpConfirmationRequestSubject
               .hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'selectGmailOtpConfirmationRequestHandler()!');
        resolve({
          response: {
            permissionGranted: false,
            errorReason: GmailOtpErrorReason.REQUEST_PROMISE_NO_SUBSCRIBER,
          },
        });
        return;
      }
      const requestWithCallback: GmailOtpConfirmationRequest = {
        ...payload.request,
        onDialogClosed: (response) => {
          resolve({response});
        },
      };
      this.actorHost.selectGmailOtpConfirmationRequestSubject.next(
          requestWithCallback);
    });
  }
}


export class GlicBrowserHostJournalImpl implements GlicBrowserHostJournal {
  constructor(private sender: PostMessageRemote<ActorHost>) {}

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
