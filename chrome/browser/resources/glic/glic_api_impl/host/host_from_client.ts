// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the client, usually passing them on
// to the browser via mojo.

import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {ContentSettingsType} from '../../content_settings_types.mojom-webui.js';
import type {ActorTaskPauseReason as ActorTaskPauseReasonMojo, ActorTaskStopReason as ActorTaskStopReasonMojo, CaptureRegionObserver, CaptureRegionResult as CaptureRegionResultMojo, OpenSettingsOptions as OpenSettingsOptionsMojo, PinCandidate as PinCandidateMojo, PinCandidatesObserver, ScrollToSelector as ScrollToSelectorMojo, WebClientHandlerInterface} from '../../glic.mojom-webui.js';
import {CaptureRegionErrorReason as CaptureRegionErrorReasonMojo, CaptureRegionObserverReceiver, CurrentView as CurrentViewMojo, PinCandidatesObserverReceiver, ResponseStopCause as ResponseStopCauseMojo, SettingsPageField as SettingsPageFieldMojo, WebClientReceiver} from '../../glic.mojom-webui.js';
import type {ActorTaskPauseReason, ActorTaskStopReason, CaptureRegionErrorReason, ConversationInfo, DraggableArea, GetPinCandidatesOptions, Journal, OnResponseStoppedDetails, OpenSettingsOptions, Screenshot, ScrollToParams, TabContextOptions, TaskOptions, ViewChangedNotification, WebClientMode, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {CaptureScreenshotErrorReason, ClientView, CreateTaskErrorReason, PerformActionsErrorReason, ResponseStopCause, ScrollToErrorReason} from '../../glic_api/glic_api.js';
import {replaceProperties} from '../conversions.js';
import {ResponseExtras} from '../post_message_transport.js';
import type {HostRequestTypes, RequestRequestType, RequestResponseType, ResumeActorTaskResultPrivate, RgbaImage, TabContextResultPrivate, TransferableException, WebClientInitialStatePrivate} from '../request_types.js';
import {ErrorWithReasonImpl, exceptionFromTransferable} from '../request_types.js';

import {bitmapN32ToRGBAImage, byteArrayFromClient, captureRegionResultToClient, focusedTabDataToClient, getArrayBufferFromBigBuffer, getPinCandidatesOptionsFromClient, hostCapabilitiesToClient, idFromClient, idToClient, optionalFromClient, optionalToClient, panelStateToClient, platformToClient, resumeActorTaskResultToClient, tabContextOptionsFromClient, tabContextToClient, tabDataToClient, taskOptionsToMojo, timeDeltaFromClient, urlFromClient, urlToClient, webClientModeToMojo} from './conversions.js';
import type {GatedSender} from './gated_sender.js';
import type {ApiHostEmbedder, GlicApiHost} from './glic_api_host.js';
import {DetailedWebClientState} from './glic_api_host.js';
import {WebClientImpl} from './host_to_client.js';


// Turn everything except void into a promise.
type Promisify<T> = T extends void ? void : Promise<T>;

// A type which the host should implement. This helps verify that
// `HostMessageHandler` is implemented with the correct parameter and return
// types.
type HostMessageHandlerInterface = {
  [Property in keyof HostRequestTypes as string extends Property ? never :
                                                                   Property]:
      // `payload` is the message payload.
  (payload: RequestRequestType<Property>, extras: ResponseExtras) =>
      Promisify<RequestResponseType<Property>>;
};

/**
 * Handles all requests to the host.
 *
 * Each function is a message handler, automatically called when the host
 * receives a message with the corresponding request name.
 *
 * Any new state or function that's not a handler should be added to
 * `GlicApiHost`.
 */
export class HostMessageHandler implements HostMessageHandlerInterface {
  // Undefined until the web client is initialized.
  private receiver: WebClientReceiver|undefined;

  // Reminder: Don't add more state here! See `HostMessageHandler`'s comment.

  constructor(
      private handler: WebClientHandlerInterface, private sender: GatedSender,
      private embedder: ApiHostEmbedder, private host: GlicApiHost) {}

  destroy() {
    if (this.receiver) {
      this.receiver.$.close();
      this.receiver = undefined;
    }
  }

  async glicBrowserWebClientCreated(_request: void, extras: ResponseExtras):
      Promise<{initialState: WebClientInitialStatePrivate}> {
    if (this.receiver) {
      throw new Error('web client already created');
    }
    this.host.detailedWebClientState =
        DetailedWebClientState.WEB_CLIENT_NOT_INITIALIZED;

    const webClientImpl = new WebClientImpl(this.host, this.embedder);
    this.receiver = new WebClientReceiver(webClientImpl);
    const {initialState} = await this.handler.webClientCreated(
        this.receiver.$.bindNewPipeAndPassRemote());
    this.host.setInitialState(initialState);
    const chromeVersion = initialState.chromeVersion.components;
    const hostCapabilities = initialState.hostCapabilities;
    this.host.setInstanceIsActive(initialState.instanceIsActive);
    const platform = initialState.platform;

    // If the panel isn't active, don't send the focused tab until later.
    if (initialState.enableApiActivationGating && !initialState.panelIsActive) {
      const actualFocus = initialState.focusedTabData;
      initialState.focusedTabData = {
        noFocusedTabData: {
          activeTabData: null,
          noFocusReason: 'glic not active',
        },
      };
      // Note: this will queue up the message, and not send it right awway.
      webClientImpl.notifyFocusedTabChanged(actualFocus);
    }

    return {
      initialState: replaceProperties(initialState, {
        panelState: panelStateToClient(initialState.panelState),
        focusedTabData:
            focusedTabDataToClient(initialState.focusedTabData, extras),
        chromeVersion: {
          major: chromeVersion[0] || 0,
          minor: chromeVersion[1] || 0,
          build: chromeVersion[2] || 0,
          patch: chromeVersion[3] || 0,
        },
        platform: platformToClient(platform),
        loggingEnabled: loadTimeData.getBoolean('loggingEnabled'),
        hostCapabilities: hostCapabilitiesToClient(hostCapabilities),
      }),
    };
  }

  glicBrowserWebClientInitialized(
      request: {success: boolean, exception?: TransferableException}) {
    // The webview may have been re-shown by webui, having previously been
    // opened by the browser. In that case, show the guest frame again.

    if (request.exception) {
      console.warn(exceptionFromTransferable(request.exception));
    }

    if (request.success) {
      this.handler.webClientInitialized();
      this.host.webClientInitialized();
    } else {
      this.handler.webClientInitializeFailed();
      this.host.webClientInitializeFailed();
    }
  }

  async glicBrowserCreateTab(request: {
    url: string,
    options: {openInBackground?: boolean, windowId?: string},
  }) {
    const response = await this.handler.createTab(
        urlFromClient(request.url),
        request.options.openInBackground !== undefined ?
            request.options.openInBackground :
            false,
        idFromClient(request.options.windowId));
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

  glicBrowserOpenGlicSettingsPage(request: {options?: OpenSettingsOptions}):
      void {
    const optionsMojo: OpenSettingsOptionsMojo = {
      highlightField: SettingsPageFieldMojo.kNone,
    };
    if (request.options?.highlightField) {
      optionsMojo.highlightField = request.options?.highlightField as number;
    }
    this.handler.openGlicSettingsPage(optionsMojo);
  }

  glicBrowserOpenPasswordManagerSettingsPage(): void {
    this.handler.openPasswordManagerSettingsPage();
  }

  glicBrowserClosePanel(): void {
    return this.handler.closePanel();
  }

  glicBrowserClosePanelAndShutdown(): void {
    this.handler.closePanelAndShutdown();
  }

  glicBrowserAttachPanel(): void {
    this.handler.attachPanel();
  }

  glicBrowserDetachPanel(): void {
    this.handler.detachPanel();
  }

  glicBrowserShowProfilePicker(): void {
    this.handler.showProfilePicker();
  }

  glicBrowserGetModelQualityClientId():
      Promise<{modelQualityClientId: string}> {
    return this.handler.getModelQualityClientId();
  }

  async glicBrowserSwitchConversation(request: {info?: ConversationInfo}):
      Promise<{}> {
    const {errorReason} =
        await this.handler.switchConversation(request.info ?? null);
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'switchConversation', errorReason.valueOf());
    }
    return {};
  }

  async glicBrowserRegisterConversation(request: {info: ConversationInfo}):
      Promise<{}> {
    const {errorReason} = await this.handler.registerConversation(request.info);
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'registerConversation', errorReason.valueOf());
    }
    return {};
  }

  async glicBrowserGetContextFromFocusedTab(
      request: {options: TabContextOptions}, extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromFocusedTab(
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async glicBrowserGetContextFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromTab(
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

  async glicBrowserGetContextForActorFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextForActorFromTab(
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

  async glicBrowserSetMaximumNumberOfPinnedTabs(request: {
    requestedMax: number,
  }): Promise<{effectiveMax: number}> {
    const requestedMax = request.requestedMax >= 0 ? request.requestedMax : 0;
    const {effectiveMax} =
        await this.handler.setMaximumNumberOfPinnedTabs(requestedMax);
    return {effectiveMax};
  }

  async glicBrowserCreateTask(request: {taskOptions?: TaskOptions}):
      Promise<{taskId: number}> {
    try {
      const taskId =
          await this.handler.createTask(taskOptionsToMojo(request.taskOptions));
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
      const resultProto = await this.handler.performActions(
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

  glicBrowserStopActorTask(
      request: {taskId: number, stopReason: ActorTaskStopReason}): void {
    const actorTaskStopReason =
        request.stopReason as number as ActorTaskStopReasonMojo;
    this.handler.stopActorTask(request.taskId, actorTaskStopReason);
  }

  glicBrowserPauseActorTask(request: {
    taskId: number,
    pauseReason: ActorTaskPauseReason,
    tabId: string,
  }): void {
    const actorTaskPauseReason =
        request.pauseReason as number as ActorTaskPauseReasonMojo;
    this.handler.pauseActorTask(
        request.taskId, actorTaskPauseReason, idFromClient(request.tabId));
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
        await this.handler.resumeActorTask(
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
  }): void {
    this.handler.interruptActorTask(request.taskId);
  }

  glicBrowserUninterruptActorTask(request: {
    taskId: number,
  }): void {
    this.handler.uninterruptActorTask(request.taskId);
  }

  async glicBrowserCreateActorTab(request: {
    taskId: number,
    options: {
      initiatorTabId?: string,
      initiatorWindowId?: string,
      openInBackground?: boolean,
    },
  }) {
    const response = await this.handler.createActorTab(
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

  glicBrowserActivateTab(request: {tabId: string}): void {
    this.handler.activateTab(idFromClient(request.tabId));
  }

  async glicBrowserResizeWindow(request: {
    size: {width: number, height: number},
    options?: {durationMs?: number},
  }) {
    this.embedder.onGuestResizeRequest(request.size);
    return await this.handler.resizeWidget(
        request.size, timeDeltaFromClient(request.options?.durationMs));
  }

  glicBrowserEnableDragResize(request: {enabled: boolean}) {
    return this.embedder.enableDragResize(request.enabled);
  }

  glicBrowserSubscribeToCaptureRegion(request: {observationId: number}): void {
    this.host.captureRegionObserver?.destroy();
    this.host.captureRegionObserver = new CaptureRegionObserverImpl(
        this.sender, this.handler, request.observationId);
  }

  glicBrowserUnsubscribeFromCaptureRegion(request: {observationId: number}):
      void {
    if (!this.host.captureRegionObserver) {
      return;
    }
    if (this.host.captureRegionObserver.observationId ===
        request.observationId) {
      this.host.captureRegionObserver.destroy();
      this.host.captureRegionObserver = undefined;
    }
  }

  async glicBrowserCaptureScreenshot(_request: void, extras: ResponseExtras):
      Promise<{screenshot: Screenshot}> {
    const {
      result: {screenshot, errorReason},
    } = await this.handler.captureScreenshot();
    if (!screenshot) {
      throw new ErrorWithReasonImpl(
          'captureScreenshot',
          (errorReason as CaptureScreenshotErrorReason | undefined) ??
              CaptureScreenshotErrorReason.UNKNOWN);
    }
    const screenshotArray = new Uint8Array(screenshot.data);
    extras.addTransfer(screenshotArray.buffer);
    return {
      screenshot: {
        widthPixels: screenshot.widthPixels,
        heightPixels: screenshot.heightPixels,
        data: screenshotArray.buffer,
        mimeType: screenshot.mimeType,
        originAnnotations: {},
      },
    };
  }

  glicBrowserSetWindowDraggableAreas(request: {areas: DraggableArea[]}) {
    return this.handler.setPanelDraggableAreas(request.areas);
  }

  glicBrowserSetMinimumWidgetSize(request: {
    size: {width: number, height: number},
  }) {
    return this.handler.setMinimumPanelSize(request.size);
  }

  glicBrowserSetMicrophonePermissionState(request: {enabled: boolean}) {
    return this.handler.setMicrophonePermissionState(request.enabled);
  }

  glicBrowserSetLocationPermissionState(request: {enabled: boolean}) {
    return this.handler.setLocationPermissionState(request.enabled);
  }

  glicBrowserSetTabContextPermissionState(request: {enabled: boolean}) {
    return this.handler.setTabContextPermissionState(request.enabled);
  }

  glicBrowserSetClosedCaptioningSetting(request: {enabled: boolean}) {
    return this.handler.setClosedCaptioningSetting(request.enabled);
  }

  glicBrowserSetActuationOnWebSetting(request: {enabled: boolean}) {
    return this.handler.setActuationOnWebSetting(request.enabled);
  }

  async glicBrowserGetUserProfileInfo(_request: void, extras: ResponseExtras) {
    const {profileInfo: mojoProfileInfo} =
        await this.handler.getUserProfileInfo();
    if (!mojoProfileInfo) {
      return {};
    }

    let avatarIcon: RgbaImage|undefined;
    if (mojoProfileInfo.avatarIcon) {
      avatarIcon = bitmapN32ToRGBAImage(mojoProfileInfo.avatarIcon);
      if (avatarIcon) {
        extras.addTransfer(avatarIcon.dataRGBA);
      }
    }
    return {profileInfo: replaceProperties(mojoProfileInfo, {avatarIcon})};
  }

  glicBrowserRefreshSignInCookies(): Promise<{success: boolean}> {
    return this.handler.syncCookies();
  }

  glicBrowserSetContextAccessIndicator(request: {show: boolean}): void {
    this.handler.setContextAccessIndicator(request.show);
  }

  glicBrowserSetAudioDucking(request: {enabled: boolean}): void {
    this.handler.setAudioDucking(request.enabled);
  }

  glicBrowserOnUserInputSubmitted(request: {mode: number}): void {
    this.handler.onUserInputSubmitted(request.mode);
  }

  glicBrowserOnContextUploadStarted(): void {
    this.handler.onContextUploadStarted();
  }

  glicBrowserOnContextUploadCompleted(): void {
    this.handler.onContextUploadCompleted();
  }

  glicBrowserOnReaction(request: {reactionType: number}): void {
    this.handler.onReaction(request.reactionType);
  }

  glicBrowserOnResponseStarted(): void {
    this.handler.onResponseStarted();
  }

  glicBrowserOnResponseStopped(request: {details?: OnResponseStoppedDetails}):
      void {
    const cause = request.details?.cause;

    let causeMojo = ResponseStopCauseMojo.kUnknown;
    if (cause !== undefined) {
      switch (cause) {
        case ResponseStopCause.USER:
          causeMojo = ResponseStopCauseMojo.kUser;
          break;
        case ResponseStopCause.OTHER:
          causeMojo = ResponseStopCauseMojo.kOther;
          break;
        default:
          assertNotReached();
      }
    }
    this.handler.onResponseStopped({cause: causeMojo});
  }

  glicBrowserOnSessionTerminated(): void {
    this.handler.onSessionTerminated();
  }

  glicBrowserOnTurnCompleted(request: {model: number, duration: number}): void {
    this.handler.onTurnCompleted(
        request.model, timeDeltaFromClient(request.duration));
  }

  glicBrowserOnModelChanged(request: {model: number}): void {
    this.handler.onModelChanged(request.model);
  }

  glicBrowserOnRecordUseCounter(request: {counter: number}): void {
    this.handler.onRecordUseCounter(request.counter);
  }

  glicBrowserLogBeginAsyncEvent(request: {
    asyncEventId: number,
    taskId: number,
    event: string,
    details: string,
  }): void {
    this.handler.logBeginAsyncEvent(
        BigInt(request.asyncEventId), request.taskId, request.event,
        request.details);
  }

  glicBrowserLogEndAsyncEvent(request: {asyncEventId: number, details: string}):
      void {
    this.handler.logEndAsyncEvent(
        BigInt(request.asyncEventId), request.details);
  }

  glicBrowserLogInstantEvent(
      request: {taskId: number, event: string, details: string}): void {
    this.handler.logInstantEvent(
        request.taskId, request.event, request.details);
  }

  glicBrowserJournalClear(): void {
    this.handler.journalClear();
  }

  async glicBrowserJournalSnapshot(
      request: {clear: boolean},
      extras: ResponseExtras): Promise<{journal: Journal}> {
    const result = await this.handler.journalSnapshot(request.clear);
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
    this.handler.journalStart(
        BigInt(request.maxBytes), request.captureScreenshots);
  }

  glicBrowserJournalStop(): void {
    this.handler.journalStop();
  }

  glicBrowserJournalRecordFeedback(
      request: {positive: boolean, reason: string}): void {
    this.handler.journalRecordFeedback(request.positive, request.reason);
  }

  glicBrowserOnResponseRated(request: {positive: boolean}): void {
    this.handler.onResponseRated(request.positive);
  }

  glicBrowserOnClosedCaptionsShown(): void {
    this.handler.onClosedCaptionsShown();
  }

  async glicBrowserScrollTo(request: {params: ScrollToParams}) {
    const {params} = request;

    function getMojoSelector(): ScrollToSelectorMojo {
      const {selector} = params;
      if (selector.exactText !== undefined) {
        if (selector.exactText.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          exactTextSelector: {
            text: selector.exactText.text,
            searchRangeStartNodeId:
                selector.exactText.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.textFragment !== undefined) {
        if (selector.textFragment.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          textFragmentSelector: {
            textStart: selector.textFragment.textStart,
            textEnd: selector.textFragment.textEnd,
            searchRangeStartNodeId:
                selector.textFragment.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.node !== undefined) {
        if (params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'nodeId without documentId');
        }
        return {
          nodeSelector: {
            nodeId: selector.node.nodeId,
          },
        };
      }
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    }

    const mojoParams = {
      highlight: params.highlight === undefined ? true : params.highlight,
      selector: getMojoSelector(),
      documentId: params.documentId ?? null,
      url: params.url ? urlFromClient(params.url) : null,
    };
    const {errorReason} = (await this.handler.scrollTo(mojoParams));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl('scrollTo', errorReason as number);
    }
    return {};
  }

  glicBrowserSetSyntheticExperimentState(request: {
    trialName: string,
    groupName: string,
  }) {
    return this.handler.setSyntheticExperimentState(
        request.trialName, request.groupName);
  }

  glicBrowserOpenOsPermissionSettingsMenu(request: {permission: string}) {
    // Warning: calling openOsPermissionSettingsMenu with unsupported content
    // setting type will terminate the render process (bad mojo message). Update
    // GlicWebClientHandler:OpenOsPermissionSettingsMenu with any new types.
    switch (request.permission) {
      case 'media':
        return this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.MEDIASTREAM_MIC);
      case 'geolocation':
        return this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.GEOLOCATION);
    }
    return Promise.resolve();
  }

  glicBrowserGetOsMicrophonePermissionStatus(): Promise<{enabled: boolean}> {
    return this.handler.getOsMicrophonePermissionStatus();
  }

  glicBrowserPinTabs(request: {tabIds: string[]}):
      Promise<{pinnedAll: boolean}> {
    return this.handler.pinTabs(request.tabIds.map((x) => idFromClient(x)));
  }

  glicBrowserUnpinTabs(request: {tabIds: string[]}):
      Promise<{unpinnedAll: boolean}> {
    return this.handler.unpinTabs(request.tabIds.map((x) => idFromClient(x)));
  }

  glicBrowserUnpinAllTabs(): void {
    this.handler.unpinAllTabs();
  }

  glicBrowserSubscribeToPinCandidates(request: {
    options: GetPinCandidatesOptions,
    observationId: number,
  }): void {
    this.host.pinCandidatesObserver?.disconnectFromSource();
    this.host.pinCandidatesObserver = new PinCandidatesObserverImpl(
        this.sender, this.handler, request.options, request.observationId);
  }

  glicBrowserUnsubscribeFromPinCandidates(request: {observationId: number}):
      void {
    if (!this.host.pinCandidatesObserver) {
      return;
    }
    if (this.host.pinCandidatesObserver.observationId ===
        request.observationId) {
      this.host.pinCandidatesObserver.disconnectFromSource();
      this.host.pinCandidatesObserver = undefined;
    }
  }

  async glicBrowserGetZeroStateSuggestionsForFocusedTab(request: {
    isFirstRun?: boolean,
  }): Promise<{suggestions?: ZeroStateSuggestions}> {
    const zeroStateResult =
        await this.handler.getZeroStateSuggestionsForFocusedTab(
            optionalFromClient(request.isFirstRun));
    const zeroStateData = zeroStateResult.suggestions;
    if (!zeroStateData) {
      return {};
    } else {
      return {
        suggestions: {
          tabId: idToClient(zeroStateData.tabId),
          url: urlToClient(zeroStateData.tabUrl),
          suggestions: zeroStateData.suggestions,
        },
      };
    }
  }

  async glicBrowserGetZeroStateSuggestionsAndSubscribe(request: {
    hasActiveSubscription: boolean,
    options: ZeroStateSuggestionsOptions,
  }): Promise<{suggestions?: ZeroStateSuggestionsV2}> {
    const zeroStateResult =
        await this.handler.getZeroStateSuggestionsAndSubscribe(
            request.hasActiveSubscription, {
              isFirstRun: request.options.isFirstRun ?? false,
              supportedTools: request.options.supportedTools ?? [],
            });
    const zeroStateData = zeroStateResult.zeroStateSuggestions;
    if (!zeroStateData) {
      return {};
    } else {
      return {suggestions: zeroStateData};
    }
  }
  glicBrowserDropScrollToHighlight(): void {
    this.handler.dropScrollToHighlight();
  }

  glicBrowserMaybeRefreshUserStatus(): void {
    this.handler.maybeRefreshUserStatus();
  }

  glicBrowserOnViewChanged(request: {notification: ViewChangedNotification}):
      void {
    const {currentView} = request.notification;
    switch (currentView) {
      case ClientView.ACTUATION:
        this.handler.onViewChanged({currentView: CurrentViewMojo.kActuation});
        break;
      case ClientView.CONVERSATION:
        this.handler.onViewChanged(
            {currentView: CurrentViewMojo.kConversation});
        break;
      default:
        // The compiler should enforce that this is unreachable if types are
        // correct; nonetheless check at runtime since TypeScript cannot
        // guarantee this absolutely.
        const _exhaustive: never = currentView;
        throw new Error(
            `glicBrowserOnViewChanged: invalid currentView: ${_exhaustive}`);
    }
  }

  glicBrowserSubscribeToPageMetadata(request: {
    tabId: string,
    names: string[],
  }): Promise<{success: boolean}> {
    return this.handler.subscribeToPageMetadata(
        idFromClient(request.tabId), request.names);
  }

  glicBrowserOnModeChange(request: {newMode: WebClientMode}): void {
    this.handler.onModeChange(webClientModeToMojo(request.newMode));
  }

  glicBrowserSetOnboardingCompleted(): void {
    this.handler.setOnboardingCompleted();
  }

  // TODO(crbug.com/458761731): Function parameters is prefixed with "_" to
  // bypass compiler error on variables declared but never used. Remove once
  // function body is implemented.
  async glicBrowserLoadAndExtractContent(
      _request: {
        urls: string[],
        options: TabContextOptions[],
      },
      _extras: ResponseExtras): Promise<{results: TabContextResultPrivate[]}> {
    // TODO(crbug.com/458761731): Once `loadAndExtractContent` is defined in the
    // handler interface, call `this.handler.loadAndExtractContent` to get the
    // response, then return the tab context to client.

    return Promise.reject(new Error('Not implemented'));
  }
}


export class CaptureRegionObserverImpl implements CaptureRegionObserver {
  receiver?: CaptureRegionObserverReceiver;
  constructor(
      private sender: GatedSender, private handler: WebClientHandlerInterface,
      public observationId: number) {
    this.connectToSource();
  }

  // Stops requesting updates.
  destroy() {
    if (!this.receiver) {
      return;
    }
    this.receiver.$.close();
    this.receiver = undefined;
  }

  // Starts requesting updates.
  private connectToSource() {
    if (this.receiver) {
      return;
    }
    this.receiver = new CaptureRegionObserverReceiver(this);
    const remote = this.receiver.$.bindNewPipeAndPassRemote();
    this.receiver.onConnectionError.addListener(() => {
      // The connection was closed without OnUpdate being called with an error.
      this.onUpdate(null, CaptureRegionErrorReasonMojo.kUnknown);
    });
    this.handler.captureRegion(remote);
  }

  onUpdate(
      result: CaptureRegionResultMojo|null,
      reason: CaptureRegionErrorReasonMojo|null): void {
    const captureResult = captureRegionResultToClient(result);
    if (captureResult) {
      // Use `sendWhenActive` to queue up all captured regions if the panel is
      // inactive. This is important because the panel is inactive during region
      // selection, and we don't want to lose any of the user's selections.
      this.sender.sendWhenActive('glicWebClientCaptureRegionUpdate', {
        result: captureResult,
        observationId: this.observationId,
      });
    } else {
      // Use `sendWhenActive` to ensure the error is delivered, even if the
      // panel is currently inactive.
      this.sender.sendWhenActive('glicWebClientCaptureRegionUpdate', {
        reason: (reason ?? CaptureRegionErrorReasonMojo.kUnknown) as number as
            CaptureRegionErrorReason,
        observationId: this.observationId,
      });
      this.destroy();
    }
  }
}

export class PinCandidatesObserverImpl implements PinCandidatesObserver {
  receiver?: PinCandidatesObserverReceiver;
  constructor(
      private sender: GatedSender, private handler: WebClientHandlerInterface,
      private options: GetPinCandidatesOptions, public observationId: number) {
    this.connectToSource();
  }

  // Stops requesting updates. This should be called on destruction, as well as
  // when the panel is hidden to avoid incurring unnecessary costs.
  disconnectFromSource() {
    if (!this.receiver) {
      return;
    }
    this.receiver.$.close();
    this.receiver = undefined;
  }

  // Start/resume requesting updates.
  connectToSource() {
    if (this.receiver) {
      return;
    }
    this.receiver = new PinCandidatesObserverReceiver(this);
    this.handler.subscribeToPinCandidates(
        getPinCandidatesOptionsFromClient(this.options),
        this.receiver.$.bindNewPipeAndPassRemote());
  }

  onPinCandidatesChanged(candidates: PinCandidateMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientPinCandidatesChanged', {
          candidates:
              candidates.map(c => ({
                               tabData: tabDataToClient(c.tabData, extras),
                             })),
          observationId: this.observationId,
        },
        extras.transfers);
  }
}
