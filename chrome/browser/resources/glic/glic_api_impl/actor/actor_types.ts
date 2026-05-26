// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActorTaskInterruptReason, ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, AutofillSuggestion, CancelActionsResult, Credential, FormFillingRequest, FormFillingResponse, Journal, NavigationConfirmationRequest, NavigationConfirmationResponse, SelectAutofillSuggestionsDialogRequest, SelectAutofillSuggestionsDialogResponse, SelectCredentialDialogRequest, SelectCredentialDialogResponse, TabContextOptions, TaskOptions, UserConfirmationDialogRequest, UserConfirmationDialogResponse} from '../../glic_api/glic_api.js';
import type {ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate} from '../request_types.js';
import type {ValidateRequestMap} from '../transport/messaging.js';

// Shared between host and client.

export declare interface ActorHost {
  glicBrowserGetContextForActorFromTab: {
    request: {
      tabId: string,
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
    backgroundAllowed: true,
  };
  glicBrowserCreateTask: {
    request: {
      taskOptions?: TaskOptions,
    },
    response: {
      taskId: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserPerformActions: {
    request: {
      actions: ArrayBuffer,
    },
    response: {
      actionsResult: ArrayBuffer,
    },
    backgroundAllowed: true,
  };
  glicBrowserCancelActions: {
    request: {
      taskId: number,
    },
    response: {
      result: CancelActionsResult,
    },
    backgroundAllowed: true,
  };
  glicBrowserStopActorTask: {
    request: {
      taskId: number,
      stopReason: ActorTaskStopReason,
    },
    backgroundAllowed: true,
  };
  glicBrowserPauseActorTask: {
    request: {
      taskId: number,
      pauseReason: ActorTaskPauseReason,
      tabId: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserResumeActorTask: {
    request: {
      taskId: number,
      tabContextOptions: TabContextOptions,
    },
    response: {
      resumeActorTaskResult: ResumeActorTaskResultPrivate,
    },
    backgroundAllowed: true,
  };
  glicBrowserInterruptActorTask: {
    request: {
      taskId: number,
      interruptReason?: ActorTaskInterruptReason,
    },
    backgroundAllowed: true,
  };
  glicBrowserUninterruptActorTask: {
    request: {
      taskId: number,
    },
    backgroundAllowed: true,
  };
  glicBrowserCreateActorTab: {
    request: {
      taskId: number,
      options: {
        initiatorTabId?: string,
        initiatorWindowId?: string,
        openInBackground?: boolean,
      },
    },
    response: {
      // Undefined on failure.
      tabData?: TabDataPrivate,
    },
    backgroundAllowed: true,
  };
  glicBrowserLogBeginAsyncEvent: {
    request: {
      asyncEventId: number,
      taskId: number,
      event: string,
      details: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserLogEndAsyncEvent: {
    request: {
      asyncEventId: number,
      details: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserLogInstantEvent: {
    request: {
      taskId: number,
      event: string,
      details: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserJournalClear: {
    backgroundAllowed: true,
  };
  glicBrowserJournalSnapshot: {
    request: {
      clear: boolean,
    },
    response: {
      journal: Journal,
    },
    backgroundAllowed: true,
  };
  glicBrowserJournalStart: {
    request: {
      maxBytes: number,
      captureScreenshots: boolean,
    },
    backgroundAllowed: true,
  };
  glicBrowserJournalStop: {
    backgroundAllowed: true,
  };
  glicBrowserJournalRecordFeedback: {
    request: {
      positive: boolean,
      reason: string,
    },
    backgroundAllowed: true,
  };
  glicBrowserAutofillSuggestionDialogOnFormPresented: {
    request: {
      taskId: number,
      params: {formFillingRequestIndex: number},
    },
    backgroundAllowed: true,
  };
  glicBrowserAutofillSuggestionDialogOnFormPreviewChanged: {
    request: {
      taskId: number,
      params: {
        formFillingRequestIndex: number,
        response?: FormFillingResponse,
      },
    },
    backgroundAllowed: true,
  };
  glicBrowserAutofillSuggestionDialogOnFormConfirmed: {
    request: {
      taskId: number,
      params: {
        formFillingRequestIndex: number,
        response: FormFillingResponse,
      },
    },
    backgroundAllowed: true,
  };
}
export type CheckActorHost = ValidateRequestMap<ActorHost>;

export declare interface ActorClient {
  glicWebClientNotifyActorTaskStateChanged: {
    request: {
      taskId: number,
      state: ActorTaskState,
    },
    backgroundAllowed: true,
  };
  glicWebClientRequestToShowDialog: {
    request: {
      request: SelectCredentialDialogRequestPrivate,
    },
    response: {
      response: SelectCredentialDialogResponsePrivate,
    },
    backgroundAllowed: true,
  };
  glicWebClientRequestToShowConfirmationDialog: {
    request: {
      request: UserConfirmationDialogRequestPrivate,
    },
    response: {
      response: UserConfirmationDialogResponsePrivate,
    },
    backgroundAllowed: true,
  };
  glicWebClientRequestToConfirmNavigation: {
    request: {
      request: NavigationConfirmationRequestPrivate,
    },
    response: {
      response: NavigationConfirmationResponsePrivate,
    },
    backgroundAllowed: true,
  };
  glicWebClientRequestToShowAutofillSuggestionsDialog: {
    request: {
      request: SelectAutofillSuggestionsDialogRequestPrivate,
    },
    response: {
      response: SelectAutofillSuggestionsDialogResponsePrivate,
    },
    backgroundAllowed: true,
  };
}
export type CheckActorClient = ValidateRequestMap<ActorClient>;

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
