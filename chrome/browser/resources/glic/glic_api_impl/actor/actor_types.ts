// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActorTaskInterruptReason, ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, AutofillSuggestion, CancelActionsResult, Credential, FormFillingRequest, FormFillingResponse, Journal, NavigationConfirmationRequest, NavigationConfirmationResponse, SelectAutofillSuggestionsDialogRequest, SelectAutofillSuggestionsDialogResponse, SelectCredentialDialogRequest, SelectCredentialDialogResponse, TabContextOptions, TaskOptions, UserConfirmationDialogRequest, UserConfirmationDialogResponse} from '../../glic_api/glic_api.js';
import type {ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate} from '../request_types.js';
import {defInterface, defMessage} from '../transport/messaging.js';

// Shared between host and client.

export const ActorHostDef = defInterface({
  name: 'ActorHost',
  methods: [
    {
      name: 'getContextForActorFromTab',
      request: defMessage<{
        tabId: string,
        options: TabContextOptions,
      }>(),
      response: defMessage<{
        tabContextResult: TabContextResultPrivate,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 11,
      },
    },
    {
      name: 'createTask',
      request: defMessage<{
        taskOptions?: TaskOptions,
      }>(),
      response: defMessage<{
        taskId: number,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 60,
      },
    },
    {
      name: 'performActions',
      request: defMessage<{
        actions: ArrayBuffer,
      }>(),
      response: defMessage<{
        actionsResult: ArrayBuffer,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 61,
      },
    },
    {
      name: 'cancelActions',
      request: defMessage<{
        taskId: number,
      }>(),
      response: defMessage<{
        result: CancelActionsResult,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 85,
      },
    },
    {
      name: 'stopActorTask',
      request: defMessage<{
        taskId: number,
        stopReason: ActorTaskStopReason,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 13,
      },
    },
    {
      name: 'pauseActorTask',
      request: defMessage<{
        taskId: number,
        pauseReason: ActorTaskPauseReason,
        tabId: string,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 14,
      },
    },
    {
      name: 'resumeActorTask',
      request: defMessage<{
        taskId: number,
        tabContextOptions: TabContextOptions,
      }>(),
      response: defMessage<{
        resumeActorTaskResult: ResumeActorTaskResultPrivate,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 15,
      },
    },
    {
      name: 'interruptActorTask',
      request: defMessage<{
        taskId: number,
        interruptReason?: ActorTaskInterruptReason,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 74,
      },
    },
    {
      name: 'uninterruptActorTask',
      request: defMessage<{
        taskId: number,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 75,
      },
    },
    {
      name: 'createActorTab',
      request: defMessage<{
        taskId: number,
        options: {
          initiatorTabId?: string,
          initiatorWindowId?: string,
          openInBackground?: boolean,
        },
      }>(),
      response: defMessage<{
        // Undefined on failure.
        tabData?: TabDataPrivate,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 77,
      },
    },
    {
      name: 'logBeginAsyncEvent',
      request: defMessage<{
        asyncEventId: number,
        taskId: number,
        event: string,
        details: string,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 30,
      },
    },
    {
      name: 'logEndAsyncEvent',
      request: defMessage<{
        asyncEventId: number,
        details: string,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 31,
      },
    },
    {
      name: 'logInstantEvent',
      request: defMessage<{
        taskId: number,
        event: string,
        details: string,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 32,
      },
    },
    {
      name: 'journalClear',
      backgroundAllowed: true,
      histogram: {
        id: 33,
      },
    },
    {
      name: 'journalSnapshot',
      request: defMessage<{
        clear: boolean,
      }>(),
      response: defMessage<{
        journal: Journal,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 34,
      },
    },
    {
      name: 'journalStart',
      request: defMessage<{
        maxBytes: number,
        captureScreenshots: boolean,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 35,
      },
    },
    {
      name: 'journalStop',
      backgroundAllowed: true,
      histogram: {
        id: 36,
      },
    },
    {
      name: 'journalRecordFeedback',
      request: defMessage<{
        positive: boolean,
        reason: string,
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 37,
      },
    },
    {
      name: 'autofillSuggestionDialogOnFormPresented',
      request: defMessage<{
        taskId: number,
        params: {formFillingRequestIndex: number},
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 87,
      },
    },
    {
      name: 'autofillSuggestionDialogOnFormPreviewChanged',
      request: defMessage<{
        taskId: number,
        params: {
          formFillingRequestIndex: number,
          response?: FormFillingResponse,
        },
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 88,
      },
    },
    {
      name: 'autofillSuggestionDialogOnFormConfirmed',
      request: defMessage<{
        taskId: number,
        params: {
          formFillingRequestIndex: number,
          response: FormFillingResponse,
        },
      }>(),
      backgroundAllowed: true,
      histogram: {
        id: 89,
      },
    },
  ],
});
export type ActorHost = typeof ActorHostDef;

export const ActorClientDef = defInterface({
  name: 'ActorClient',
  methods: [
    {
      name: 'glicWebClientNotifyActorTaskStateChanged',
      request: defMessage<{
        taskId: number,
        state: ActorTaskState,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientRequestToShowDialog',
      request: defMessage<{
        request: SelectCredentialDialogRequestPrivate,
      }>(),
      response: defMessage<{
        response: SelectCredentialDialogResponsePrivate,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientRequestToShowConfirmationDialog',
      request: defMessage<{
        request: UserConfirmationDialogRequestPrivate,
      }>(),
      response: defMessage<{
        response: UserConfirmationDialogResponsePrivate,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientRequestToConfirmNavigation',
      request: defMessage<{
        request: NavigationConfirmationRequestPrivate,
      }>(),
      response: defMessage<{
        response: NavigationConfirmationResponsePrivate,
      }>(),
      backgroundAllowed: true,
    },
    {
      name: 'glicWebClientRequestToShowAutofillSuggestionsDialog',
      request: defMessage<{
        request: SelectAutofillSuggestionsDialogRequestPrivate,
      }>(),
      response: defMessage<{
        response: SelectAutofillSuggestionsDialogResponsePrivate,
      }>(),
      backgroundAllowed: true,
    },
  ],
});

export type ActorClient = typeof ActorClientDef;

export declare interface CredentialPrivate extends
    Omit<Credential, 'getIcon'|'getAccountPicture'> {
  accountPicture?: RgbaImage;
}

export declare interface FormFillingRequestPrivate extends
    Omit<FormFillingRequest, 'suggestions'> {
  suggestions: AutofillSuggestionPrivate[];
}

export declare interface AutofillSuggestionPrivate extends
    Omit<AutofillSuggestion, 'getIcon'> {
  icon?: RgbaImage;
}

export declare interface SelectCredentialDialogRequestPrivate extends Omit<
    SelectCredentialDialogRequest, 'onDialogClosed'|'icons'|'credentials'> {
  icons: Map<string, RgbaImage>;
  credentials: CredentialPrivate[];
}

export declare interface SelectCredentialDialogResponsePrivate extends
    SelectCredentialDialogResponse {
  errorReason?: SelectCredentialDialogErrorReason;
}

export declare interface SelectAutofillSuggestionsDialogRequestPrivate extends
    Omit<
        SelectAutofillSuggestionsDialogRequest,
        'onDialogClosed'|'onFormPresented'|'onFormPreviewChanged'|
        'onFormConfirmed'|'formFillingRequests'> {
  taskId: number;
  formFillingRequests: FormFillingRequestPrivate[];
}

export declare interface UserConfirmationDialogRequestPrivate extends
    Omit<UserConfirmationDialogRequest, 'onDialogClosed'> {}


export declare interface SelectAutofillSuggestionsDialogResponsePrivate extends
    SelectAutofillSuggestionsDialogResponse {
  taskId: number;
  errorReason?: SelectAutofillSuggestionsDialogErrorReason;
}

export declare interface NavigationConfirmationRequestPrivate extends
    Omit<NavigationConfirmationRequest, 'onConfirmationDecision'> {}

export declare interface NavigationConfirmationResponsePrivate extends
    NavigationConfirmationResponse {
  errorReason?: ConfirmationRequestErrorReason;
}

export enum ConfirmationRequestErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  REQUEST_PROMISE_NO_SUBSCRIBER = 0,
  // The task requested a new user confirmation dialog before the current
  // one completed.
  PREEMPTED_BY_NEW_REQUEST = 1,
}

export declare interface UserConfirmationDialogResponsePrivate extends
    UserConfirmationDialogResponse {
  errorReason?: ConfirmationRequestErrorReason;
}

/** Reasons why the credential selection dialog request failed. */
export enum SelectCredentialDialogErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  DIALOG_PROMISE_NO_SUBSCRIBER = 0,
}

// LINT.IfChange(SelectAutofillSuggestionsDialogErrorReason)
/** Reasons why the autofill suggestion selection dialog request failed. */
export enum SelectAutofillSuggestionsDialogErrorReason {
  // The hosting WebUI received the request, but the web client has not
  // subscribed to the request yet. We couldn't show the dialog in this case.
  DIALOG_PROMISE_NO_SUBSCRIBER = 0,
  // The requested task id did not match the response task id. This error is
  // internal to the browser and not sent by the client over mojo.
  MISMATCHED_TASK_ID = 1,
  // The task is not connected to a delegate. I.e. attempting to run the task
  // from the experimental actor API. This error is internal to the browser and
  // not sent by the client over mojo.
  NO_ACTOR_TASK_DELEGATE = 2,
}
// LINT.ThenChange(//chrome/common/actor_webui.mojom:SelectAutofillSuggestionsDialogErrorReason)
