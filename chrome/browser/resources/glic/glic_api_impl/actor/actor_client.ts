// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Web client side actor message handler.

import {assert} from '//resources/js/assert.js';

import type * as actorWebUiMojom from '../../actor_webui.mojom-webui.js';
import {GmailOtpErrorReason as GmailOtpErrorReasonMojo, SelectAutofillSuggestionsDialogErrorReason as SelectAutofillSuggestionsDialogErrorReasonMojo, SelectCredentialDialogErrorReason as SelectCredentialDialogErrorReasonMojo} from '../../actor_webui.mojom-webui.js';
import type {GmailOtpConfirmationRequest as GmailOtpConfirmationRequestMojo, GmailOtpConfirmationResult as GmailOtpConfirmationResultMojo, GmailOtpOptInRequest as GmailOtpOptInRequestMojo, GmailOtpOptInResult as GmailOtpOptInResultMojo, NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, TaskOptions as TaskOptionsMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo, UserGrantedPermissionDuration as UserGrantedPermissionDurationMojo} from '../../actor_webui.mojom-webui.js';
import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import {ActorClientReceiver, ActorHandlerRemote} from '../../glic.mojom-webui.js';
import type {ActorClientInterface, ActorTaskState as ActorTaskStateMojo, TabContextResult as TabContextMojo, WebClientHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import type * as glicApi from '../../glic_api/glic_api.js';
import type {ActorTaskInterruptReason, CancelActionsResult, CreateActorTabOptions, Credential, FormFillingResponse, GlicBrowserHost, GlicBrowserHostJournal, GmailOtpConfirmationRequest, GmailOtpOptInRequest, Journal, NavigationConfirmationRequest, Observable, ObservableValue, ResumeActorTaskResult, SelectAutofillSuggestionsDialogRequest, SelectCredentialDialogRequest, TabContextOptions, TabContextResult, TabData, TaskOptions, UserConfirmationDialogRequest} from '../../glic_api/glic_api.js';
import {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, CreateTaskErrorReason, FeatureMode, PerformActionsErrorReason} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl, Subject} from '../../observable.js';
import {convertTabContextResultFromPrivate, convertTabDataFromPrivate} from '../client/glic_api_client.js';
import {rgbaImageToBlob} from '../client/image_utils.js';
import type {CheckEnumCompatibility} from '../conversions.js';
import {bitmapN32ToRGBAImage, byteArrayFromClient, getArrayBufferFromBigBuffer, idFromClient, idToClient, optionalFromClient, optionalToClient, originToClient, tabContextOptionsFromClient, tabContextToClient, urlToClient} from '../host/conversions.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';
import type {ResumeActorTaskResultPrivate} from '../request_types.js';
import {ErrorWithReasonImpl} from '../request_types.js';
import {assertNever, ResponseExtras} from '../transport/messaging.js';

// Implements actor-specific methods on GlicBrowserHost.
export class GlicBrowserHostActor implements ActorClientInterface,
                                             Partial<GlicBrowserHost> {
  private actorHandler?: ActorHandlerRemote;
  private actorClientReceiver?: ActorClientReceiver;

  readonly userConfirmationDialogRequestSubject =
      new Subject<UserConfirmationDialogRequest>();
  readonly selectCredentialDialogRequestSubject =
      new Subject<SelectCredentialDialogRequest>();
  readonly navigationConfirmationRequestSubject =
      new Subject<NavigationConfirmationRequest>();
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

  initialize(
      initialState: WebClientInitialState, handler: WebClientHandlerRemote) {
    if (!initialState.enableActInFocusedTab || !handler) {
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

    this.actorHandler = maybeWrapWithLogging(
        new ActorHandlerRemote(), {prefix: 'ActorHandler'});
    this.actorClientReceiver = new ActorClientReceiver(this);
    handler.createActorHandler(
        this.actorHandler.$.bindNewPipeAndPassReceiver(),
        this.actorClientReceiver.$.bindNewPipeAndPassRemote());
    this.journalHost = new GlicBrowserHostJournalImpl(this.actorHandler);

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

  destroyActor(): void {
    if (this.actorHandler) {
      this.actorHandler.$.close();
      this.actorHandler = undefined;
    }
    if (this.actorClientReceiver) {
      this.actorClientReceiver.$.close();
      this.actorClientReceiver = undefined;
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
    this.actorHandler?.autofillSuggestionDialogOnFormPresented(taskId, params);
  }

  autofillSuggestionDialogOnFormPreviewChanged(taskId: number, params: {
    formFillingRequestIndex: number,
    response?: FormFillingResponse,
  }): void {
    this.actorHandler?.autofillSuggestionDialogOnFormPreviewChanged(taskId, {
      formFillingRequestIndex: params.formFillingRequestIndex,
      response: params.response ?? null,
    });
  }

  autofillSuggestionDialogOnFormConfirmed(taskId: number, params: {
    formFillingRequestIndex: number,
    response: FormFillingResponse,
  }): void {
    this.actorHandler?.autofillSuggestionDialogOnFormConfirmed(taskId, params);
  }

  async getContextForActorFromTab?
      (tabId: string, options: TabContextOptions): Promise<TabContextResult> {
    assert(this.actorHandler);
    const {result: {errorReason, tabContext}} =
        await this.actorHandler.getContextForActorFromTab(
            idFromClient(tabId), tabContextOptionsFromClient(options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult =
        tabContextToClient(tabContext, new ResponseExtras());
    return convertTabContextResultFromPrivate(tabContextResult);
  }

  async createTask?(taskOptions?: TaskOptions): Promise<number> {
    assert(this.actorHandler);
    try {
      return await this.actorHandler.createTask(taskOptionsToMojo(taskOptions));
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'createTask',
          (errorReason as CreateTaskErrorReason | undefined) ??
              CreateTaskErrorReason.UNKNOWN);
    }
  }

  async performActions?(actions: ArrayBuffer): Promise<ArrayBuffer> {
    assert(this.actorHandler);
    try {
      const resultProto =
          await this.actorHandler.performActions(byteArrayFromClient(actions));
      const buffer = getArrayBufferFromBigBuffer(resultProto.smuggled);
      if (!buffer) {
        throw PerformActionsErrorReason.UNKNOWN;
      }
      return buffer;
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'performActions',
          (errorReason as PerformActionsErrorReason | undefined) ??
              PerformActionsErrorReason.UNKNOWN);
    }
  }

  async cancelActions?(taskId: number): Promise<CancelActionsResult> {
    assert(this.actorHandler);
    const cancelResult = await this.actorHandler.cancelActions(taskId);
    return enumToClient(cancelResult.result);
  }

  stopActorTask?(taskId?: number, stopReason?: ActorTaskStopReason): void {
    this.actorHandler?.stopActorTask(
        taskId ?? 0,
        enumFromClient(stopReason ?? ActorTaskStopReason.TASK_COMPLETE));
  }

  pauseActorTask?
      (taskId: number, pauseReason?: ActorTaskPauseReason, tabId?: string):
          void {
    this.actorHandler?.pauseActorTask(
        taskId,
        enumFromClient(pauseReason ?? ActorTaskPauseReason.PAUSED_BY_MODEL),
        idFromClient(tabId));
  }

  async resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
      Promise<ResumeActorTaskResult> {
    assert(this.actorHandler);
    const {
      result: {
        getContextResult,
        actionResult,
      },
    } =
        await this.actorHandler.resumeActorTask(
            taskId, tabContextOptionsFromClient(tabContextOptions));
    if (!getContextResult.tabContext || actionResult === null) {
      throw new Error(
          `resumeActorTask failed: ${getContextResult.errorReason}`);
    }
    const extras = new ResponseExtras();
    const resumeActorTaskResult = resumeActorTaskResultToClient(
        getContextResult.tabContext, actionResult, extras);
    return convertTabContextResultFromPrivate(resumeActorTaskResult);
  }

  interruptActorTask?
      (taskId: number, interruptReason?: ActorTaskInterruptReason): void {
    this.actorHandler?.interruptActorTask(
        taskId, enumFromClient(interruptReason));
  }

  uninterruptActorTask?(taskId: number): void {
    this.actorHandler?.uninterruptActorTask(taskId);
  }

  updateActorTaskStepProgress?(taskId: number, stepProgress: string): void {
    this.actorHandler?.updateActorTaskStepProgress(taskId, stepProgress);
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
    assert(this.actorHandler);
    const response = await this.actorHandler.createActorTab(taskId, {
      initiatorTabId: idFromClient(options.initiatorTabId),
      initiatorWindowId: idFromClient(options.initiatorWindowId),
      openInBackground: options.openInBackground === true,
    });
    const tabData = response.tabData;
    if (tabData) {
      return convertTabDataFromPrivate({
        tabId: idToClient(tabData.tabId),
        windowId: idToClient(tabData.windowId),
        url: urlToClient(tabData.url),
        title: optionalToClient(tabData.title),
      });
    }
    throw new Error('createActorTab: failed');
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

  // ActorClientInterface implementation:
  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    this.setActorTaskState(taskId, enumToClient(state));
  }

  async requestToShowCredentialSelectionDialog(
      request: SelectCredentialDialogRequestMojo):
      Promise<{response: SelectCredentialDialogResponseMojo}> {
    return new Promise(resolve => {
      if (!this.selectCredentialDialogRequestSubject.hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'selectCredentialDialogRequest()!');
        resolve({
          response: {
            taskId: request.taskId,
            errorReason: SelectCredentialDialogErrorReasonMojo
                             .kDialogPromiseNoSubscriber,
            permissionDuration: null,
            selectedCredentialId: null,
          },
        });
        return;
      }

      const iconsGetter = new Map<string, () => Promise<Blob>>();
      if (request.icons) {
        for (const [id, value] of Object.entries(request.icons)) {
          let promise: Promise<Blob>|undefined;
          iconsGetter.set(id, () => {
            if (!promise) {
              const rgbaImage = bitmapN32ToRGBAImage(value);
              promise = Promise.resolve(
                  rgbaImage ? rgbaImageToBlob(rgbaImage) : new Blob());
            }
            return promise;
          });
        }
      }

      const credentials: Credential[] = request.credentials.map(credential => {
        const getIcon = iconsGetter.get(credential.sourceSiteOrApp);
        const accountPicture = credential.accountPicture ?
            bitmapN32ToRGBAImage(credential.accountPicture) :
            undefined;
        const getAccountPicture = accountPicture ?
            () => Promise.resolve(rgbaImageToBlob(accountPicture)) :
            undefined;
        return {
          id: credential.id,
          username: credential.username,
          sourceSiteOrApp: credential.sourceSiteOrApp,
          requestOrigin: originToClient(credential.requestOrigin),
          type: enumToClient(credential.type),
          getIcon,
          getAccountPicture,
        };
      });

      const requestWithCallback: SelectCredentialDialogRequest = {
        taskId: request.taskId,
        showDialog: request.showDialog,
        credentials,
        onDialogClosed: (result) => {
          resolve({
            response: {
              taskId: request.taskId,
              errorReason: null,
              permissionDuration:
                  optionalFromClient(result.response.permissionDuration) as
                      UserGrantedPermissionDurationMojo |
                  null,
              selectedCredentialId:
                  result.response.selectedCredentialId ?? null,
            },
          });
        },
      };
      this.selectCredentialDialogRequestSubject.next(requestWithCallback);
    });
  }

  requestToShowUserConfirmationDialog(
      request: UserConfirmationDialogRequestMojo):
      Promise<{response: UserConfirmationDialogResponseMojo}> {
    return new Promise(resolve => {
      if (!this.userConfirmationDialogRequestSubject.hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'userConfirmationDialogRequest()!');
        resolve({
          response: {
            result: {
              permissionGranted: false,
            },
          },
        });
        return;
      }
      const requestWithCallback: UserConfirmationDialogRequest = {
        navigationOrigin: request.payload.navigationOrigin ?
            originToClient(request.payload.navigationOrigin) :
            undefined,
        forBlocklistedOrigin: request.payload.forBlocklistedOrigin,
        onDialogClosed: (result) => {
          resolve({
            response: {
              result: {
                permissionGranted: result.response.permissionGranted,
              },
            },
          });
        },
      };
      this.userConfirmationDialogRequestSubject.next(requestWithCallback);
    });
  }

  requestToConfirmNavigation(request: NavigationConfirmationRequestMojo):
      Promise<{response: NavigationConfirmationResponseMojo}> {
    return new Promise(resolve => {
      if (!this.navigationConfirmationRequestSubject.hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'navigationConfirmationRequest()!');
        resolve({
          response: {
            result: {
              permissionGranted: false,
            },
          },
        });
        return;
      }
      const requestWithCallback: NavigationConfirmationRequest = {
        taskId: request.taskId,
        navigationOrigin: originToClient(request.navigationOrigin),
        onConfirmationDecision: (result) => {
          resolve({
            response: {
              result: {
                permissionGranted: result.response.permissionGranted ?? false,
              },
            },
          });
        },
      };
      this.navigationConfirmationRequestSubject.next(requestWithCallback);
    });
  }

  requestToShowAutofillSuggestionsDialog(
      request: SelectAutofillSuggestionsDialogRequestMojo):
      Promise<{response: SelectAutofillSuggestionsDialogResponseMojo}> {
    return new Promise(resolve => {
      if (!this.selectAutofillSuggestionsDialogRequestSubject
               .hasActiveSubscription()) {
        resolve({
          response: {
            taskId: request.taskId,
            result: {
              errorReason: SelectAutofillSuggestionsDialogErrorReasonMojo
                               .kDialogPromiseNoSubscriber,
            },
          },
        });
        return;
      }
      const requestWithCallback: SelectAutofillSuggestionsDialogRequest = {
        formFillingRequests: request.formFillingRequests.map(
            formFillingRequest => ({
              requestedData: Number(formFillingRequest.requestedData),
              formattedRequestOrigin:
                  formFillingRequest.formattedRequestOrigin ?? undefined,
              sectionLabel: formFillingRequest.sectionLabel ?? undefined,
              suggestions: formFillingRequest.suggestions.map(suggestion => {
                const icon = suggestion.icon ?
                    bitmapN32ToRGBAImage(suggestion.icon) :
                    undefined;
                const getIcon = icon ?
                    () => Promise.resolve(rgbaImageToBlob(icon)) :
                    undefined;
                return {
                  id: suggestion.id,
                  title: suggestion.title,
                  details: suggestion.details,
                  getIcon,
                };
              }),
            })),
        onDialogClosed: (result) => {
          resolve({
            response: {
              taskId: request.taskId,
              result: {
                selectedSuggestions: result.response.selectedSuggestions,
              },
            },
          });
        },
        onFormPresented: (params) => {
          this.autofillSuggestionDialogOnFormPresented(request.taskId, params);
        },
        onFormPreviewChanged: (params) => {
          this.autofillSuggestionDialogOnFormPreviewChanged(
              request.taskId, params);
        },
        onFormConfirmed: (params) => {
          this.autofillSuggestionDialogOnFormConfirmed(request.taskId, params);
        },
      };
      this.selectAutofillSuggestionsDialogRequestSubject.next(
          requestWithCallback);
    });
  }

  requestToShowGmailOtpOptInDialog(request: GmailOtpOptInRequestMojo):
      Promise<{result: GmailOtpOptInResultMojo}> {
    return new Promise(resolve => {
      if (!this.selectGmailOtpOptInRequestSubject.hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'selectGmailOtpOptInRequestHandler()!');
        resolve({
          result: {
            errorReason: GmailOtpErrorReasonMojo.kRequestPromiseNoSubscriber,
          },
        });
        return;
      }
      const requestWithCallback: GmailOtpOptInRequest = {
        taskId: request.taskId,
        onDialogClosed: (response) => {
          resolve({
            result: {
              response: {
                permissionGranted: response.permissionGranted,
              },
            },
          });
        },
      };
      this.selectGmailOtpOptInRequestSubject.next(requestWithCallback);
    });
  }

  requestToShowGmailOtpConfirmationDialog(
      request: GmailOtpConfirmationRequestMojo):
      Promise<{result: GmailOtpConfirmationResultMojo}> {
    return new Promise(resolve => {
      if (!this.selectGmailOtpConfirmationRequestSubject
               .hasActiveSubscription()) {
        window.console.warn(
            'GlicWebClient: no subscriber for ' +
            'selectGmailOtpConfirmationRequestHandler()!');
        resolve({
          result: {
            errorReason: GmailOtpErrorReasonMojo.kRequestPromiseNoSubscriber,
          },
        });
        return;
      }
      const requestWithCallback: GmailOtpConfirmationRequest = {
        taskId: request.taskId,
        verificationCode: request.verificationCode,
        onDialogClosed: (response) => {
          resolve({
            result: {
              response: {
                permissionGranted: response.permissionGranted,
              },
            },
          });
        },
      };
      this.selectGmailOtpConfirmationRequestSubject.next(requestWithCallback);
    });
  }
}

export class GlicBrowserHostJournalImpl implements GlicBrowserHostJournal {
  constructor(private actorHandler: ActorHandlerRemote) {}

  beginAsyncEvent(
      asyncEventId: number, taskId: number, event: string,
      details: string): void {
    this.actorHandler.logBeginAsyncEvent(
        BigInt(asyncEventId), taskId, event, details);
  }

  clear(): void {
    this.actorHandler.journalClear();
  }

  endAsyncEvent(asyncEventId: number, details: string): void {
    this.actorHandler.logEndAsyncEvent(BigInt(asyncEventId), details);
  }

  instantEvent(taskId: number, event: string, details: string): void {
    this.actorHandler.logInstantEvent(taskId, event, details);
  }

  async snapshot(clear: boolean): Promise<Journal> {
    const result = await this.actorHandler.journalSnapshot(clear);
    const journalArray = new Uint8Array(result.journal.data);
    return {
      data: journalArray.buffer,
    };
  }

  start(maxBytes: number, captureScreenshots: boolean): void {
    this.actorHandler.journalStart(BigInt(maxBytes), captureScreenshots);
  }

  stop(): void {
    this.actorHandler.journalStop();
  }

  recordFeedback(positive: boolean, reason: string): void {
    this.actorHandler.journalRecordFeedback(positive, reason);
  }
}

assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.UserGrantedPermissionDuration,
    typeof glicApi.UserGrantedPermissionDuration>>();

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
