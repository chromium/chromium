// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the client, usually passing them on
// to the browser via mojo.

import {assertNotReached} from '//resources/js/assert.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';

import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import {CaptureRegionObserverReceiver, PromptType as PromptTypeMojo, ResponseStopCause as ResponseStopCauseMojo, TabDataHandlerReceiver, TabFaviconHandlerReceiver} from '../../glic.mojom-webui.js';
import type {CaptureRegionErrorReason as CaptureRegionErrorReasonMojo, CaptureRegionObserver, CaptureRegionResult as CaptureRegionResultMojo, TabDataHandlerInterface, TabDataMojoType, TabFaviconHandlerInterface, WebClientHandlerInterface} from '../../glic.mojom-webui.js';
import {CaptureScreenshotErrorReason, ResponseStopCause} from '../../glic_api/glic_api.js';
import type {CaptureRegionParams, ClientErrorDialogType, ConversationInfo, CounterAbuseVerdict, ExperimentalTriggeringUpdate, MicrophoneStatus, OnResponseStoppedDetails, OpenPinnedTabPickerOptions, PinTabsOptions, PromptType, Screenshot, TabContextOptions, UnpinTabsOptions, WebClientMode, ZeroStateSuggestions} from '../../glic_api/glic_api.js';
import {replaceProperties} from '../conversions.js';
import {getGuestLoadTimeData} from '../guest_load_time_data.js';
import type {AnnotationHost, GlicException, ImageBytesResultPrivate, RgbaImage, TabContextResultPrivate, WebClientHost, WebClientRegionCapture, WebClientTabDataObserver, WebClientTabFaviconObserver} from '../request_types.js';
import {ErrorWithReasonImpl, exceptionFromTransferable, SubscriberObservationType} from '../request_types.js';
import {ResponseExtras} from '../transport/messaging.js';
import type {PendingReceiver, PendingRemote, PostMessageHandler, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {bitmapN32ToRGBAImage, captureRegionResultToClient, conversationInfoFromClient, counterAbuseVerdictFromClient, idFromClient, idToClient, imageBytesResultToClient, microphoneStatusToMojo, openPinnedTabPickerOptionsToMojo, optionalFromClient, pinTabsOptionsToMojo, subscriberObservationTypeFromClient, tabContextOptionsFromClient, tabContextToClient, tabDataToPrivate, timeDeltaFromClient, unpinTabsOptionsToMojo, urlToClient, webClientModeToMojo} from './conversions.js';
import type {GlicApiHost} from './glic_api_host.js';
import {linkPipeClosure} from './host_utils.js';

/**
 * Handles all requests to the host.
 *
 * Each function is a message handler, automatically called when the host
 * receives a message with the corresponding request name.
 *
 * Any new state or function that's not a handler should be added to
 * `GlicApiHost`.
 */
export class HostMessageHandler implements PostMessageHandler<WebClientHost> {
  private enableStructuredYieldMetadata: boolean|null = null;

  // Reminder: Don't add more state here! See `HostMessageHandler`'s comment.
  constructor(
      private handler: WebClientHandlerInterface, private host: GlicApiHost) {}

  destroy() {}

  createAnnotationHandler(
      request: {annotationReceiver: PendingReceiver<AnnotationHost>},
      _extras: ResponseExtras): void {
    this.host.createAnnotationHandler(request.annotationReceiver);
  }

  webClientInitialized(request: {success: boolean, exception?: GlicException}) {
    // The webview may have been re-shown by webui, having previously been
    // opened by the browser. In that case, show the guest frame again.

    if (request.exception) {
      console.warn(exceptionFromTransferable(request.exception));
    }

    if (request.success) {
      this.host.webClientInitialized();
    } else {
      this.host.webClientInitializeFailed();
    }
  }

  onExperimentalTriggeringUpdate(payload: {
    observationId: number,
    update?: ExperimentalTriggeringUpdate,
          observation: SubscriberObservationType,
  }) {
    const handler = this.host.getExperimentalTriggeringUpdatesHandler(
        payload.observationId);
    if (handler) {
      if (this.enableStructuredYieldMetadata === null) {
        this.enableStructuredYieldMetadata =
            getGuestLoadTimeData().enableStructuredYieldMetadata ?? false;
      }
      handler.onUpdate(
          payload.update ? {
            type: enumFromClient(payload.update.type),
            data: payload.update.data,
            metadata: this.enableStructuredYieldMetadata ?
                optionalFromClient(payload.update.metadata) :
                null,
          } :
                           null,
          subscriberObservationTypeFromClient(payload.observation));

      if (payload.observation === SubscriberObservationType.COMPLETE ||
          payload.observation === SubscriberObservationType.ERROR) {
        this.host.deleteExperimentalTriggeringUpdatesHandler(
            payload.observationId);
      }
    }
  }

  reportClientTransientError(request: {abslStatus: number}): void {
    this.handler.reportClientTransientError(request.abslStatus);
  }

  processCounterAbuseVerdict(request: {
    tabId: string,
    verdict: CounterAbuseVerdict,
  }): void {
    const mojoVerdict = counterAbuseVerdictFromClient(request.verdict);
    this.handler.processCounterAbuseVerdict(
        idFromClient(request.tabId), mojoVerdict);
  }


  getModelQualityClientId(): Promise<{modelQualityClientId: string}> {
    return this.handler.getModelQualityClientId();
  }

  async switchConversation(request: {info?: ConversationInfo}): Promise<{}> {
    const {errorReason} = await this.handler.switchConversation(
        conversationInfoFromClient(request.info ?? {
          conversationId: '',
          conversationTitle: '',
          clientData: undefined,
        }));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'switchConversation', errorReason.valueOf());
    }
    return {};
  }

  async registerConversation(request: {info: ConversationInfo}): Promise<{}> {
    const {errorReason} = await this.handler.registerConversation(
        conversationInfoFromClient(request.info));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'registerConversation', errorReason.valueOf());
    }
    return {};
  }

  async getContextFromFocusedTab(
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

  async getContextFromTab(
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

  async getImageBytesFromTab(
      request: {tabId: string, documentId: string, domNodeId: number},
      extras: ResponseExtras):
      Promise<{result: ImageBytesResultPrivate | null}> {
    const {result: {errorReason, imageBytes}} =
        await this.handler.getImageBytesFromTab(
            idFromClient(request.tabId), request.documentId, request.domNodeId);
    if (!imageBytes) {
      throw new Error(`getImageBytes failed: ${errorReason}`);
    }
    return {
      result: imageBytesResultToClient(imageBytes, extras),
    };
  }

  async setMaximumNumberOfPinnedTabs(request: {
    requestedMax: number,
  }): Promise<{effectiveMax: number}> {
    const requestedMax = request.requestedMax >= 0 ? request.requestedMax : 0;
    const {effectiveMax} =
        await this.handler.setMaximumNumberOfPinnedTabs(requestedMax);
    return {effectiveMax};
  }

  activateTab(request: {tabId: string}): void {
    this.handler.activateTab(idFromClient(request.tabId));
  }

  async resizeWindow(request: {
    size: {width: number, height: number},
    options?: {durationMs?: number},
  }) {
    return await this.handler.resizeWidget(
        request.size, timeDeltaFromClient(request.options?.durationMs));
  }

  enableDragResize(request: {enabled: boolean}) {
    return this.handler.enableDragResize(request.enabled);
  }

  subscribeToCaptureRegion(request: {
    remote: PendingRemote<WebClientRegionCapture>,
    params?: CaptureRegionParams,
  }): void {
    this.host.captureRegionObserver?.destroy();
    const remote: PostMessageRemote<WebClientRegionCapture> =
        this.host.router.newRemote(request.remote);
    const observer =
        new CaptureRegionObserverImpl(remote, this.handler, request.params);
    remote.addCloseHandler(() => {
      observer.destroy();
      if (this.host.captureRegionObserver === observer) {
        this.host.captureRegionObserver = undefined;
      }
    });
    this.host.captureRegionObserver = observer;
  }

  deleteCapturedRegion(request: {tabId: string, regionId: string}) {
    this.handler.deleteCapturedRegion(
        idFromClient(request.tabId), request.regionId);
  }

  async captureScreenshot(_request: void, extras: ResponseExtras):
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

  setMinimumWidgetSize(request: {
    size: {width: number, height: number},
  }) {
    return this.handler.setMinimumPanelSize(request.size);
  }

  setMicrophonePermissionState(request: {enabled: boolean}) {
    return this.handler.setMicrophonePermissionState(request.enabled);
  }

  setLocationPermissionState(request: {enabled: boolean}) {
    return this.handler.setLocationPermissionState(request.enabled);
  }

  setTabContextPermissionState(request: {enabled: boolean}) {
    return this.handler.setTabContextPermissionState(request.enabled);
  }

  setClosedCaptioningSetting(request: {enabled: boolean}) {
    return this.handler.setClosedCaptioningSetting(request.enabled);
  }

  setActuationOnWebSetting(request: {enabled: boolean}) {
    return this.handler.setActuationOnWebSetting(request.enabled);
  }

  async getUserProfileInfo(_request: void, extras: ResponseExtras) {
    const {profileInfo: mojoProfileInfo} =
        await this.handler.getUserProfileInfo();
    if (!mojoProfileInfo) {
      return {};
    }

    let avatarIcon: RgbaImage|undefined;
    bitmapN32ToRGBAImage;
    if (mojoProfileInfo.avatarIcon) {
      avatarIcon = bitmapN32ToRGBAImage(mojoProfileInfo.avatarIcon);
      if (avatarIcon) {
        extras.addTransfer(avatarIcon.dataRGBA);
      }
    }
    return {profileInfo: replaceProperties(mojoProfileInfo, {avatarIcon})};
  }

  refreshSignInCookies(): Promise<{success: boolean}> {
    return this.handler.syncCookies();
  }

  setContextAccessIndicator(request: {show: boolean}): void {
    this.handler.setContextAccessIndicator(request.show);
  }

  setAudioDucking(request: {enabled: boolean}): void {
    this.handler.setAudioDucking(request.enabled);
  }

  onOptinImpression(): void {
    this.handler.onOptinImpression();
  }

  onUserInputSubmitted(request: {mode: number, promptType?: PromptType}): void {
    this.handler.onUserInputSubmitted(
        webClientModeToMojo(request.mode),
        enumFromClient(request.promptType) ?? PromptTypeMojo.kUnspecified);
  }

  onContextUploadStarted(): void {
    this.handler.onContextUploadStarted();
  }

  onContextUploadCompleted(): void {
    this.handler.onContextUploadCompleted();
  }

  onReaction(request: {reactionType: number}): void {
    this.handler.onReaction(request.reactionType);
  }

  onActionSubmitted(request: {isRetry?: boolean}): void {
    this.handler.onActionSubmitted(request.isRetry ?? false);
  }

  onResponseStarted(): void {
    this.handler.onResponseStarted();
  }

  onResponseStopped(request: {details?: OnResponseStoppedDetails}): void {
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

  onSessionTerminated(): void {
    this.handler.onSessionTerminated();
  }

  onTurnCompleted(request: {model: number, duration: number}): void {
    this.handler.onTurnCompleted(
        request.model, timeDeltaFromClient(request.duration));
  }

  recordHistogram(request: {name: string, sparseValue: number}): void {
    this.handler.recordSparseValue(request.name, request.sparseValue);
  }
  onResponseRated(request: {positive: boolean}): void {
    this.handler.onResponseRated(request.positive);
  }

  onClosedCaptionsShown(): void {
    this.handler.onClosedCaptionsShown();
  }


  setSyntheticExperimentState(request: {
    trialName: string,
    groupName: string,
  }) {
    return this.handler.setSyntheticExperimentState(
        request.trialName, request.groupName);
  }


  getOsMicrophonePermissionStatus(): Promise<{enabled: boolean}> {
    return this.handler.getOsMicrophonePermissionStatus();
  }

  pinTabs(request: {tabIds: string[], options?: PinTabsOptions}):
      Promise<{pinnedAll: boolean}> {
    return this.handler.pinTabs(
        request.tabIds.map((x) => idFromClient(x)),
        pinTabsOptionsToMojo(request.options));
  }

  unpinTabs(request: {tabIds: string[], options?: UnpinTabsOptions}):
      Promise<{unpinnedAll: boolean}> {
    return this.handler.unpinTabs(
        request.tabIds.map((x) => idFromClient(x)),
        unpinTabsOptionsToMojo(request.options));
  }

  unpinAllTabs(request: {options?: UnpinTabsOptions}): void {
    this.handler.unpinAllTabs(unpinTabsOptionsToMojo(request.options));
  }

  async openPinnedTabPicker(request: {options?: OpenPinnedTabPickerOptions}):
      Promise<void> {
    await this.handler.openPinnedTabPicker(
        openPinnedTabPickerOptionsToMojo(request.options));
  }

  async getZeroStateSuggestionsForFocusedTab(request: {
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
          url: urlToClient(zeroStateData.url),
          suggestions: zeroStateData.suggestions,
        },
      };
    }
  }

  maybeRefreshUserStatus(): void {
    this.handler.maybeRefreshUserStatus();
  }

  subscribeToPageMetadata(request: {
    tabId: string,
    names: string[],
  }): Promise<{success: boolean}> {
    return this.handler.subscribeToPageMetadata(
        idFromClient(request.tabId), request.names);
  }

  onModeChange(request: {newMode: WebClientMode}): void {
    this.handler.onModeChange(webClientModeToMojo(request.newMode));
  }

  onMicrophoneStatusChange(request: {status: MicrophoneStatus}): void {
    this.handler.onMicrophoneStatusChange(
        microphoneStatusToMojo(request.status));
  }

  setOnboardingCompleted(): void {
    this.handler.setOnboardingCompleted();
  }

  subscribeToTabData(request: {
    tabId: string,
    remote: PendingRemote<WebClientTabDataObserver>,
  }): void {
    new TabDataHandlerImpl(
        idFromClient(request.tabId), this.handler, request.remote,
        this.host.router);
  }

  subscribeToTabFavicon(request: {
    tabId: string,
    remote: PendingRemote<WebClientTabFaviconObserver>,
  }): void {
    new TabFaviconHandlerImpl(
        idFromClient(request.tabId), this.handler, request.remote,
        this.host.router);
  }

  setErrorDialogState(request: {
    shownDialogType?: ClientErrorDialogType,
  }): void {
    if (request.shownDialogType !== undefined) {
      this.handler.clientErrorDialogStateChanged(
          enumFromClient(request.shownDialogType));
    }
    // TODO(b/506142920): Avoid showing error panels to the user if it is
    // presented while the panel is backgrounded. Automatically reload the
    // page instead.
  }
}


export class CaptureRegionObserverImpl implements CaptureRegionObserver {
  private mojoReceiver?: CaptureRegionObserverReceiver;
  constructor(
      public readonly pmRemote: PostMessageRemote<WebClientRegionCapture>,
      private handler: WebClientHandlerInterface,
      private params?: CaptureRegionParams) {
    this.mojoReceiver = new CaptureRegionObserverReceiver(this);
    linkPipeClosure(this.pmRemote, this.mojoReceiver);
    const remote = this.mojoReceiver.$.bindNewPipeAndPassRemote();
    this.handler.captureRegion(
        remote,
        this.params ? {
          tabId: idFromClient(this.params.tabId),
          options: tabContextOptionsFromClient(this.params.options),
        } :
                      null);
  }

  // Stops requesting updates.
  destroy() {
    if (!this.mojoReceiver) {
      return;
    }
    this.mojoReceiver.$.close();
    this.mojoReceiver = undefined;
  }

  onUpdate(
      result: CaptureRegionResultMojo|null,
      reason: CaptureRegionErrorReasonMojo|null): void {
    const captureResult = captureRegionResultToClient(result);
    if (captureResult) {
      this.pmRemote.requestNoResponse('captureRegionUpdate', {
        result: captureResult,
      });
    } else {
      // If the capture update failed, notify the client of the error reason
      // if provided and destroy the observer to close the pipe.
      if (reason !== null) {
        this.pmRemote.requestNoResponse('captureRegionUpdate', {
          reason: enumToClient(reason),
        });
      }
      this.destroy();
    }
  }
}

class TabDataHandlerImpl implements TabDataHandlerInterface {
  mojoReceiver?: TabDataHandlerReceiver;
  private pmRemote: PostMessageRemote<WebClientTabDataObserver>;

  constructor(
      tabId: number, handler: WebClientHandlerInterface,
      pendingRemote: PendingRemote<WebClientTabDataObserver>,
      router: PostMessageRouter) {
    this.pmRemote = router.newRemote(pendingRemote);
    this.mojoReceiver = new TabDataHandlerReceiver(this);
    linkPipeClosure(this.pmRemote, this.mojoReceiver);
    handler.subscribeToTabData(
        tabId, this.mojoReceiver.$.bindNewPipeAndPassRemote());
  }
  onTabDataChanged(tabData: TabDataMojoType): void {
    const extras = new ResponseExtras();
    this.pmRemote.requestNoResponse(
        'tabDataChanged', {
          tabData: tabDataToPrivate(tabData, extras),
        },
        extras.transfers);
  }
}

class TabFaviconHandlerImpl implements TabFaviconHandlerInterface {
  private mojoReceiver?: TabFaviconHandlerReceiver;
  private pmRemote: PostMessageRemote<WebClientTabFaviconObserver>;

  constructor(
      tabId: number, handler: WebClientHandlerInterface,
      pendingRemote: PendingRemote<WebClientTabFaviconObserver>,
      router: PostMessageRouter) {
    this.pmRemote = router.newRemote(pendingRemote);
    this.mojoReceiver = new TabFaviconHandlerReceiver(this);
    linkPipeClosure(this.pmRemote, this.mojoReceiver);
    handler.subscribeToTabFavicon(
        tabId, this.mojoReceiver.$.bindNewPipeAndPassRemote());
  }
  onTabFaviconChanged(favicon: BitmapN32|null): void {
    const extras = new ResponseExtras();
    let faviconImage: RgbaImage|undefined = undefined;
    if (favicon) {
      faviconImage = bitmapN32ToRGBAImage(favicon);
      if (faviconImage) {
        extras.addTransfer(faviconImage.dataRGBA);
      }
    }
    this.pmRemote.requestNoResponse(
        'tabFaviconChanged', {
          favicon: faviconImage,
        },
        extras.transfers);
  }
}
