// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles actor-related messages from the client, passing them on
// to the browser via mojo.

import type * as actorWebUiMojom from '../../actor_webui.mojom-webui.js';
import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import type {ActorClientInterface, ActorHandlerInterface, ActorTaskState as ActorTaskStateMojo, TabContextResult as TabContextMojo} from '../../glic.mojom-webui.js';
import type * as api from '../../glic_api/glic_api.js';
import type {ActorTaskInterruptReason, ActorTaskPauseReason, ActorTaskStopReason, CancelActionsResult, FormFillingResponse, Journal, TabContextOptions, TaskOptions} from '../../glic_api/glic_api.js';
import {CreateTaskErrorReason, FeatureMode, PerformActionsErrorReason} from '../../glic_api/glic_api.js';
import type {CheckEnumCompatibility} from '../conversions.js';
import {bitmapN32ToRGBAImage, byteArrayFromClient, getArrayBufferFromBigBuffer, idFromClient, idToClient, optionalFromClient, optionalToClient, originToClient, tabContextOptionsFromClient, tabContextToClient, urlToClient} from '../host/conversions.js';
import {ErrorWithReasonImpl} from '../request_types.js';
import type {ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate} from '../request_types.js';
import {assertNever} from '../transport/messaging.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {PostMessageHandler, PostMessageRemote} from '../transport/post_message_transport.js';

import type {ConfirmationRequestErrorReason as ConfirmationRequestErrorReasonMojo, GmailOtpConfirmationRequest as GmailOtpConfirmationRequestMojo, GmailOtpConfirmationResult as GmailOtpConfirmationResultMojo, GmailOtpErrorReason as GmailOtpErrorReasonMojo, GmailOtpOptInRequest as GmailOtpOptInRequestMojo, GmailOtpOptInResult as GmailOtpOptInResultMojo, NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogErrorReason as SelectAutofillSuggestionsDialogErrorReasonMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogErrorReason as SelectCredentialDialogErrorReasonMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, TaskOptions as TaskOptionsMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo, UserGrantedPermissionDuration as UserGrantedPermissionDurationMojo} from './../../actor_webui.mojom-webui.js';
import type * as actorTypes from './actor_types.js';
import type {ActorClient, ActorHost} from './actor_types.js';

export class ActorHostMessageHandler implements PostMessageHandler<ActorHost> {
  constructor(private actorHandler: ActorHandlerInterface) {}

  async getContextForActorFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.actorHandler.getContextForActorFromTab(
            idFromClient(request.tabId),
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async createTask(request: {taskOptions?: TaskOptions}):
      Promise<{taskId: number}> {
    try {
      const taskId = await this.actorHandler.createTask(
          taskOptionsToMojo(request.taskOptions));
      return {
        taskId: taskId,
      };
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'createTask',
          (errorReason as CreateTaskErrorReason | undefined) ??
              CreateTaskErrorReason.UNKNOWN);
    }
  }

  async performActions(request: {actions: ArrayBuffer}):
      Promise<{actionsResult: ArrayBuffer}> {
    try {
      const resultProto = await this.actorHandler.performActions(
          byteArrayFromClient(request.actions));
      const buffer = getArrayBufferFromBigBuffer(resultProto.smuggled);
      if (!buffer) {
        throw PerformActionsErrorReason.UNKNOWN;
      }
      return {
        actionsResult: buffer,
      };
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'performActions',
          (errorReason as PerformActionsErrorReason | undefined) ??
              PerformActionsErrorReason.UNKNOWN);
    }
  }

  async cancelActions(request: {taskId: number}):
      Promise<{result: CancelActionsResult}> {
    const cancelResult = await this.actorHandler.cancelActions(request.taskId);
    return {
      result: enumToClient(cancelResult.result),
    };
  }

  stopActorTask(request: {taskId: number, stopReason: ActorTaskStopReason}):
      void {
    this.actorHandler.stopActorTask(
        request.taskId, enumFromClient(request.stopReason));
  }

  pauseActorTask(request: {
    taskId: number,
    pauseReason: ActorTaskPauseReason,
    tabId: string,
  }): void {
    this.actorHandler.pauseActorTask(
        request.taskId, enumFromClient(request.pauseReason),
        idFromClient(request.tabId));
  }

  async resumeActorTask(
      request: {taskId: number, tabContextOptions: TabContextOptions},
      extras: ResponseExtras): Promise<{
    resumeActorTaskResult: ResumeActorTaskResultPrivate,
  }> {
    const {
      result: {
        getContextResult,
        actionResult,
      },
    } =
        await this.actorHandler.resumeActorTask(
            request.taskId,
            tabContextOptionsFromClient(request.tabContextOptions));
    if (!getContextResult.tabContext || actionResult === null) {
      throw new Error(
          `resumeActorTask failed: ${getContextResult.errorReason}`);
    }
    return {
      resumeActorTaskResult: resumeActorTaskResultToClient(
          getContextResult.tabContext, actionResult, extras),
    };
  }

  interruptActorTask(request: {
    taskId: number,
    interruptReason?: ActorTaskInterruptReason,
  }): void {
    this.actorHandler.interruptActorTask(
        request.taskId, enumFromClient(request.interruptReason));
  }

  uninterruptActorTask(request: {
    taskId: number,
  }): void {
    this.actorHandler.uninterruptActorTask(request.taskId);
  }

  async createActorTab(request: {
    taskId: number,
    options: api.CreateActorTabOptions,
  }) {
    const response = await this.actorHandler.createActorTab(request.taskId, {
      initiatorTabId: idFromClient(request.options.initiatorTabId),
      initiatorWindowId: idFromClient(request.options.initiatorWindowId),
      openInBackground: request.options.openInBackground === true,
    });
    const tabData = response.tabData;
    if (tabData) {
      return {
        tabData: {
          tabId: idToClient(tabData.tabId),
          windowId: idToClient(tabData.windowId),
          url: urlToClient(tabData.url),
          title: optionalToClient(tabData.title),
        },
      };
    }
    return {};
  }

  logBeginAsyncEvent(request: {
    asyncEventId: number,
    taskId: number,
    event: string,
    details: string,
  }): void {
    this.actorHandler.logBeginAsyncEvent(
        BigInt(request.asyncEventId), request.taskId, request.event,
        request.details);
  }

  logEndAsyncEvent(request: {asyncEventId: number, details: string}): void {
    this.actorHandler.logEndAsyncEvent(
        BigInt(request.asyncEventId), request.details);
  }

  logInstantEvent(request: {taskId: number, event: string, details: string}):
      void {
    this.actorHandler.logInstantEvent(
        request.taskId, request.event, request.details);
  }

  journalClear(): void {
    this.actorHandler.journalClear();
  }

  async journalSnapshot(request: {clear: boolean}, extras: ResponseExtras):
      Promise<{journal: Journal}> {
    const result = await this.actorHandler.journalSnapshot(request.clear);
    const journalArray = new Uint8Array(result.journal.data);
    extras.addTransfer(journalArray.buffer);
    return {
      journal: {
        data: journalArray.buffer,
      },
    };
  }

  journalStart(request: {maxBytes: number, captureScreenshots: boolean}): void {
    this.actorHandler.journalStart(
        BigInt(request.maxBytes), request.captureScreenshots);
  }

  journalStop(): void {
    this.actorHandler.journalStop();
  }

  journalRecordFeedback(request: {positive: boolean, reason: string}): void {
    this.actorHandler.journalRecordFeedback(request.positive, request.reason);
  }

  autofillSuggestionDialogOnFormPresented(payload: {
    taskId: number,
    params: {formFillingRequestIndex: number},
  }): void {
    this.actorHandler.autofillSuggestionDialogOnFormPresented(
        payload.taskId, payload.params);
  }

  autofillSuggestionDialogOnFormPreviewChanged(payload: {
    taskId: number,
    params: {
      formFillingRequestIndex: number,
      response?: FormFillingResponse,
    },
  }): void {
    this.actorHandler.autofillSuggestionDialogOnFormPreviewChanged(
        payload.taskId, {
          formFillingRequestIndex: payload.params.formFillingRequestIndex,
          response: payload.params.response ?? null,
        });
  }

  autofillSuggestionDialogOnFormConfirmed(payload: {
    taskId: number,
    params: {
      formFillingRequestIndex: number,
      response: FormFillingResponse,
    },
  }): void {
    this.actorHandler.autofillSuggestionDialogOnFormConfirmed(
        payload.taskId, payload.params);
  }
}


export class ActorClientImpl implements ActorClientInterface {
  constructor(private sender: PostMessageRemote<ActorClient>) {}

  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    const clientState = enumToClient(state);
    this.sender.requestNoResponse(
        'notifyActorTaskStateChanged', {taskId, state: clientState});
  }

  async requestToShowCredentialSelectionDialog(
      request: SelectCredentialDialogRequestMojo):
      Promise<{response: SelectCredentialDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToShowDialog',
        {request: selectCredentialDialogRequestToClient(request)});
    return {
      response: selectCredentialDialogResponseToMojo(clientResponse.response),
    };
  }

  async requestToShowUserConfirmationDialog(
      request: UserConfirmationDialogRequestMojo):
      Promise<{response: UserConfirmationDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToShowConfirmationDialog',
        {request: userConfirmationDialogRequestToClient(request)});
    return {
      response: userConfirmationDialogResponseToMojo(clientResponse.response),
    };
  }

  async requestToConfirmNavigation(request: NavigationConfirmationRequestMojo):
      Promise<{response: NavigationConfirmationResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToConfirmNavigation',
        {request: navigationConfirmationRequestToClient(request)});
    return {
      response: navigationConfirmationResponseToMojo(clientResponse.response),
    };
  }

  async requestToShowAutofillSuggestionsDialog(
      request: SelectAutofillSuggestionsDialogRequestMojo):
      Promise<{response: SelectAutofillSuggestionsDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToShowAutofillSuggestionsDialog',
        {request: selectAutofillSuggestionsDialogRequestToClient(request)});
    return {
      response: selectAutofillSuggestionsDialogResponseToMojo(
          clientResponse.response),
    };
  }

  async requestToShowGmailOtpOptInDialog(request: GmailOtpOptInRequestMojo):
      Promise<{result: GmailOtpOptInResultMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToShowGmailOtpOptInDialog',
        {request: gmailOtpOptInRequestToClient(request)});
    return {
      result: gmailOtpOptInResultToMojo(clientResponse.response),
    };
  }

  async requestToShowGmailOtpConfirmationDialog(
      request: GmailOtpConfirmationRequestMojo):
      Promise<{result: GmailOtpConfirmationResultMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'requestToShowGmailOtpConfirmationDialog',
        {request: gmailOtpConfirmationRequestToClient(request)});
    return {
      result: gmailOtpConfirmationResultToMojo(clientResponse.response),
    };
  }
}

assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.SelectCredentialDialogErrorReason,
    typeof actorTypes.SelectCredentialDialogErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.ConfirmationRequestErrorReason,
    typeof actorTypes.ConfirmationRequestErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.UserGrantedPermissionDuration,
    typeof api.UserGrantedPermissionDuration>>();

function selectCredentialDialogResponseToMojo(
    response: actorTypes.SelectCredentialDialogResponsePrivate):
    SelectCredentialDialogResponseMojo {
  return response.errorReason ?
      {
        taskId: response.taskId,
        errorReason: response.errorReason as number as
            SelectCredentialDialogErrorReasonMojo,
        permissionDuration: null,
        selectedCredentialId: null,
      } :
      {
        ...response,
        errorReason: null,
        permissionDuration: optionalFromClient(response.permissionDuration) as
                UserGrantedPermissionDurationMojo |
            null,
        selectedCredentialId: response.selectedCredentialId ?? null,
      };
}

function selectCredentialDialogRequestToClient(
    request: SelectCredentialDialogRequestMojo):
    actorTypes.SelectCredentialDialogRequestPrivate {
  const icons = new Map<string, RgbaImage>();
  if (request.icons) {
    for (const [siteOrApp, value] of Object.entries(request.icons)) {
      const rgbaImage = bitmapN32ToRGBAImage(value);
      if (rgbaImage) {
        icons.set(siteOrApp, rgbaImage);
      }
    }
  }
  return {
    ...request,
    credentials: request.credentials.map(
        credential => ({
          ...credential,
          requestOrigin: originToClient(credential.requestOrigin),
          type: enumToClient(credential.type),
          accountPicture: credential.accountPicture ?
              bitmapN32ToRGBAImage(credential.accountPicture) :
              undefined,
        })),
    icons,
  };
}

function userConfirmationDialogRequestToClient(
    request: UserConfirmationDialogRequestMojo):
    actorTypes.UserConfirmationDialogRequestPrivate {
  return {
    navigationOrigin: request.payload.navigationOrigin ?
        originToClient(request.payload.navigationOrigin) :
        undefined,
    forBlocklistedOrigin: request.payload.forBlocklistedOrigin,
  };
}

function userConfirmationDialogResponseToMojo(
    response: actorTypes.UserConfirmationDialogResponsePrivate):
    UserConfirmationDialogResponseMojo {
  if (response.errorReason) {
    return {
      result: {
        errorReason: response.errorReason as number as
            ConfirmationRequestErrorReasonMojo,
      },
    };
  }
  return {
    result: {permissionGranted: response.permissionGranted},
  };
}

function navigationConfirmationRequestToClient(
    request: NavigationConfirmationRequestMojo):
    actorTypes.NavigationConfirmationRequestPrivate {
  return {
    taskId: request.taskId,
    navigationOrigin: originToClient(request.navigationOrigin),
  };
}

function navigationConfirmationResponseToMojo(
    response: actorTypes.NavigationConfirmationResponsePrivate):
    NavigationConfirmationResponseMojo {
  if (response.errorReason) {
    return {
      result: {
        errorReason: response.errorReason as number as
            ConfirmationRequestErrorReasonMojo,
      },
    };
  }
  return {
    result: {
      permissionGranted: response.permissionGranted,
    },
  };
}

function selectAutofillSuggestionsDialogRequestToClient(
    request: SelectAutofillSuggestionsDialogRequestMojo):
    actorTypes.SelectAutofillSuggestionsDialogRequestPrivate {
  return {
    ...request,
    formFillingRequests: request.formFillingRequests.map(
        r => ({
          ...r,
          requestedData: Number(r.requestedData),
          formattedRequestOrigin: r.formattedRequestOrigin,
          suggestions: r.suggestions.map(
              s => ({
                ...s,
                icon: s.icon ? bitmapN32ToRGBAImage(s.icon) : undefined,
              })),
        })),
  };
}

function selectAutofillSuggestionsDialogResponseToMojo(
    response: actorTypes.SelectAutofillSuggestionsDialogResponsePrivate):
    SelectAutofillSuggestionsDialogResponseMojo {
  if (response.errorReason) {
    return {
      taskId: response.taskId,
      result: {
        errorReason: response.errorReason as number as
            SelectAutofillSuggestionsDialogErrorReasonMojo,
      },
    };
  } else {
    return {
      taskId: response.taskId,
      result: {
        selectedSuggestions: response.selectedSuggestions,
      },
    };
  }
}

function taskOptionsToMojo(taskOptions?: TaskOptions): TaskOptionsMojo|null {
  if (taskOptions) {
    return {
      title: taskOptions.title ?? null,
      duration: enumFromClient(taskOptions.duration),
      featureMode:
          enumFromClient(taskOptions.featureMode ?? FeatureMode.UNSPECIFIED),
      actuationTabId: taskOptions.actuationTabId ?
          idFromClient(taskOptions.actuationTabId) :
          null,
    };
  }
  return null;
}

function resumeActorTaskResultToClient(
    tabContext: TabContextMojo, actionResult: number,
    extras: ResponseExtras): ResumeActorTaskResultPrivate {
  return {
    ...tabContextToClient(tabContext, extras),
    actionResult,
  };
}

assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.GmailOtpErrorReason,
    typeof actorTypes.GmailOtpErrorReason>>();

function gmailOtpOptInRequestToClient(request: GmailOtpOptInRequestMojo):
    actorTypes.GmailOtpOptInRequestPrivate {
  return {
    taskId: request.taskId,
  };
}

function gmailOtpOptInResultToMojo(
    response: actorTypes.GmailOtpOptInResponsePrivate):
    GmailOtpOptInResultMojo {
  if (response.errorReason !== undefined) {
    return {
      errorReason: response.errorReason as number as GmailOtpErrorReasonMojo,
    };
  }
  return {
    response: {
      permissionGranted: response.permissionGranted,
    },
  };
}

function gmailOtpConfirmationRequestToClient(
    request: GmailOtpConfirmationRequestMojo):
    actorTypes.GmailOtpConfirmationRequestPrivate {
  return {
    taskId: request.taskId,
    verificationCode: request.verificationCode,
  };
}

function gmailOtpConfirmationResultToMojo(
    response: actorTypes.GmailOtpConfirmationResponsePrivate):
    GmailOtpConfirmationResultMojo {
  if (response.errorReason !== undefined) {
    return {
      errorReason: response.errorReason as number as GmailOtpErrorReasonMojo,
    };
  }
  return {
    response: {
      permissionGranted: response.permissionGranted,
    },
  };
}
