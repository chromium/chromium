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
import {ErrorWithReasonImpl} from '../request_types.js';
import type {ResumeActorTaskResultPrivate, TabContextResultPrivate} from '../request_types.js';
import {assertNever} from '../transport/messaging.js';
import type {MessageHandlerInterface, ResponseExtras} from '../transport/messaging.js';

import type * as actorTypes from './actor_types.js';
import type {ActorHost} from './actor_types.js';

export class ActorHostMessageHandler implements
    MessageHandlerInterface<ActorHost> {
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

assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.SelectCredentialDialogErrorReason,
    typeof actorTypes.SelectCredentialDialogErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.ConfirmationRequestErrorReason,
    typeof actorTypes.ConfirmationRequestErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.UserGrantedPermissionDuration,
    typeof api.UserGrantedPermissionDuration>>();
