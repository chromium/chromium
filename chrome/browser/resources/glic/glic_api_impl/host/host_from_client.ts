// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the client, usually passing them on
// to the browser via mojo.

import {enumFromClient} from '../../enum_conversions.js';
import type {WebClientHandlerInterface} from '../../glic.mojom-webui.js';
import type {                            //
             ClientErrorDialogType,      //
             ConversationInfo,           //
             CounterAbuseVerdict,        //
             MicrophoneStatus,           //
             OpenPinnedTabPickerOptions, //
             PinTabsOptions,             //
             TabContextOptions,          //
             UnpinTabsOptions,           //
             WebClientMode,              //
             ZeroStateSuggestions,       //
} from '../../glic_api/glic_api.js';
import {replaceProperties} from '../conversions.js';
import type {                         //
             GlicException,           //
             ImageBytesResultPrivate, //
             RgbaImage,               //
             TabContextResultPrivate, //
             WebClientHost,           //
} from '../request_types.js';
import {ErrorWithReasonImpl, exceptionFromTransferable} from '../request_types.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {PostMessageHandler} from '../transport/post_message_transport.js';

import {                             //
  bitmapN32ToRGBAImage,              //
  conversationInfoFromClient,        //
  counterAbuseVerdictFromClient,     //
  idFromClient,                      //
  idToClient,                        //
  imageBytesResultToClient,          //
  microphoneStatusToMojo,            //
  openPinnedTabPickerOptionsToMojo,  //
  optionalFromClient,                //
  pinTabsOptionsToMojo,              //
  tabContextOptionsFromClient,       //
  tabContextToClient,                //
  timeDeltaFromClient,               //
  unpinTabsOptionsToMojo,            //
  urlToClient,                       //
  webClientModeToMojo,               //
} from './conversions.js';
import type {GlicApiHost} from './glic_api_host.js';

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
  // Reminder: Don't add more state here! See `HostMessageHandler`'s comment.
  constructor(
      private handler: WebClientHandlerInterface, private host: GlicApiHost) {}

  destroy() {}

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

  deleteCapturedRegion(request: {tabId: string, regionId: string}) {
    this.handler.deleteCapturedRegion(
        idFromClient(request.tabId), request.regionId);
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


  setAudioDucking(request: {enabled: boolean}): void {
    this.handler.setAudioDucking(request.enabled);
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
