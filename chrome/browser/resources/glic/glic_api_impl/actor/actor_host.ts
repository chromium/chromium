// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles actor-related messages from the client, passing them on
// to the browser via mojo.

import type * as actorWebUiMojom from '../../actor_webui.mojom-webui.js';
import type {ActorHandlerInterface} from '../../glic.mojom-webui.js';
import type * as api from '../../glic_api/glic_api.js';
import type {ActorTaskInterruptReason, ActorTaskPauseReason, ActorTaskStopReason, CancelActionsResult, FormFillingResponse, Journal, TabContextOptions, TaskOptions} from '../../glic_api/glic_api.js';
import {CreateTaskErrorReason, PerformActionsErrorReason} from '../../glic_api/glic_api.js';
import type {CheckEnumCompatibility} from '../conversions.js';
import {enumFromClient, enumToClient} from '../enum_conversions.js';
import {byteArrayFromClient, getArrayBufferFromBigBuffer, idFromClient, idToClient, optionalToClient, resumeActorTaskResultToClient, tabContextOptionsFromClient, tabContextToClient, taskOptionsToMojo, urlToClient} from '../host/conversions.js';
import {assertNever} from '../messaging.js';
import type {ResponseExtras} from '../post_message_transport.js';
import type {MessageHandlerInterface, ResumeActorTaskResultPrivate, TabContextResultPrivate} from '../request_types.js';
import {ErrorWithReasonImpl} from '../request_types.js';

import type * as actorTypes from './actor_types.js';
import type {ActorHostRequestTypes} from './actor_types.js';

export class ActorHostMessageHandler implements
    MessageHandlerInterface<ActorHostRequestTypes> {
  constructor(private actorHandler: ActorHandlerInterface) {}

  async glicBrowserGetContextForActorFromTab(
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

  async glicBrowserCreateTask(request: {taskOptions?: TaskOptions}):
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

  async glicBrowserPerformActions(request: {actions: ArrayBuffer}):
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

  async glicBrowserCancelActions(request: {taskId: number}):
      Promise<{result: CancelActionsResult}> {
    const cancelResult = await this.actorHandler.cancelActions(request.taskId);
    return {
      result: enumToClient(cancelResult.result),
    };
  }

  glicBrowserStopActorTask(
      request: {taskId: number, stopReason: ActorTaskStopReason}): void {
    this.actorHandler.stopActorTask(
        request.taskId, enumFromClient(request.stopReason));
  }

  glicBrowserPauseActorTask(request: {
    taskId: number,
    pauseReason: ActorTaskPauseReason,
    tabId: string,
  }): void {
    this.actorHandler.pauseActorTask(
        request.taskId, enumFromClient(request.pauseReason),
        idFromClient(request.tabId));
  }

  async glicBrowserResumeActorTask(
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

  glicBrowserInterruptActorTask(request: {
    taskId: number,
    interruptReason?: ActorTaskInterruptReason,
  }): void {
    this.actorHandler.interruptActorTask(
        request.taskId, enumFromClient(request.interruptReason));
  }

  glicBrowserUninterruptActorTask(request: {
    taskId: number,
  }): void {
    this.actorHandler.uninterruptActorTask(request.taskId);
  }

  async glicBrowserCreateActorTab(request: {
    taskId: number,
    options: {
      initiatorTabId?: string,
      initiatorWindowId?: string,
      openInBackground?: boolean,
    },
  }) {
    const response = await this.actorHandler.createActorTab(
        request.taskId, request.options.openInBackground === true,
        idFromClient(request.options.initiatorTabId),
        idFromClient(request.options.initiatorWindowId));
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

  glicBrowserLogBeginAsyncEvent(request: {
    asyncEventId: number,
    taskId: number,
    event: string,
    details: string,
  }): void {
    this.actorHandler.logBeginAsyncEvent(
        BigInt(request.asyncEventId), request.taskId, request.event,
        request.details);
  }

  glicBrowserLogEndAsyncEvent(request: {asyncEventId: number, details: string}):
      void {
    this.actorHandler.logEndAsyncEvent(
        BigInt(request.asyncEventId), request.details);
  }

  glicBrowserLogInstantEvent(
      request: {taskId: number, event: string, details: string}): void {
    this.actorHandler.logInstantEvent(
        request.taskId, request.event, request.details);
  }

  glicBrowserJournalClear(): void {
    this.actorHandler.journalClear();
  }

  async glicBrowserJournalSnapshot(
      request: {clear: boolean},
      extras: ResponseExtras): Promise<{journal: Journal}> {
    const result = await this.actorHandler.journalSnapshot(request.clear);
    const journalArray = new Uint8Array(result.journal.data);
    extras.addTransfer(journalArray.buffer);
    return {
      journal: {
        data: journalArray.buffer,
      },
    };
  }

  glicBrowserJournalStart(
      request: {maxBytes: number, captureScreenshots: boolean}): void {
    this.actorHandler.journalStart(
        BigInt(request.maxBytes), request.captureScreenshots);
  }

  glicBrowserJournalStop(): void {
    this.actorHandler.journalStop();
  }

  glicBrowserJournalRecordFeedback(
      request: {positive: boolean, reason: string}): void {
    this.actorHandler.journalRecordFeedback(request.positive, request.reason);
  }

  glicBrowserAutofillSuggestionDialogOnFormPresented(payload: {
    taskId: number,
    params: {formFillingRequestIndex: number},
  }): void {
    this.actorHandler.autofillSuggestionDialogOnFormPresented(
        payload.taskId, payload.params);
  }

  glicBrowserAutofillSuggestionDialogOnFormPreviewChanged(payload: {
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

  glicBrowserAutofillSuggestionDialogOnFormConfirmed(payload: {
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

assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.SelectCredentialDialogErrorReason,
    typeof actorTypes.SelectCredentialDialogErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.ConfirmationRequestErrorReason,
    typeof actorTypes.ConfirmationRequestErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.UserGrantedPermissionDuration,
    typeof api.UserGrantedPermissionDuration>>();
