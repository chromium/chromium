// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the browser, sending messages to the client.

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import type {AdditionalContext as AdditionalContextMojo, FileUploadPolicyState, FocusedTabData as FocusedTabDataMojo, GeminiEnterpriseSettings as GeminiEnterpriseSettingsMojo, InvokeOptions as InvokeOptionsMojo, OpenPanelInfo as OpenPanelInfoMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, TabData as TabDataMojo, WebClientInterface} from '../../glic.mojom-webui.js';
import type {WebClient} from '../request_types.js';
import {ResponseExtras} from '../transport/messaging.js';
import type {PostMessageRemote} from '../transport/post_message_transport.js';

import {additionalContextToClient, fileUploadPolicyStateToClient, focusedTabDataToClient, idToClient, invokeOptionsToClient, pageMetadataToClient, panelOpeningDataToClient, panelStateToClient, tabDataToPrivate, timeDeltaFromClient, webClientModeToMojo} from './conversions.js';
import type {GlicApiHost} from './glic_api_host.js';
import {PanelOpenState} from './types.js';

export class WebClientImpl implements WebClientInterface {
  private sender: PostMessageRemote<WebClient>;
  private clientCreated = Promise.withResolvers<void>();

  constructor(private host: GlicApiHost) {
    this.sender = this.host.sender;
  }

  markCreated() {
    this.clientCreated.resolve();
  }

  async checkResponsive(): Promise<void> {
    return this.sender.requestWithResponse('checkResponsive', undefined);
  }

  async processNotifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    await this.clientCreated.promise;
    this.host.setWaitingOnPanelWillOpen(true);
    let result;
    try {
      result = await this.sender.requestWithResponse(
          'notifyPanelWillOpen',
          {panelOpeningData: panelOpeningDataToClient(panelOpeningData)});
    } finally {
      this.host.setWaitingOnPanelWillOpen(false);
      this.host.panelOpenStateChanged(PanelOpenState.OPEN);
    }

    const openPanelInfoMojo: OpenPanelInfoMojo = {
      webClientMode: webClientModeToMojo(result.openPanelInfo?.startingMode),
      panelSize: null,
      resizeDuration: timeDeltaFromClient(
          result.openPanelInfo?.resizeParams?.options?.durationMs),
      canUserResize: result.openPanelInfo?.canUserResize ?? true,
    };
    if (result.openPanelInfo?.resizeParams) {
      const size = {
        width: result.openPanelInfo?.resizeParams?.width,
        height: result.openPanelInfo?.resizeParams?.height,
      };
      openPanelInfoMojo.panelSize = size;
    }
    return {openPanelInfo: openPanelInfoMojo};
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    return this.processNotifyPanelWillOpen(panelOpeningData);
  }

  async processNotifyPanelWasClosed(): Promise<void> {
    this.host.panelOpenStateChanged(PanelOpenState.CLOSED);
    return this.sender.requestWithResponse('notifyPanelWasClosed', undefined);
  }
  notifyPanelWasClosed(): Promise<void> {
    return this.processNotifyPanelWasClosed();
  }

  invoke(options: InvokeOptionsMojo): Promise<void> {
    const extras = new ResponseExtras();
    return this.sender.requestWithResponse(
        'invoke', {
          options: invokeOptionsToClient(options, extras),
        },
        extras.transfers);
  }

  notifyPanelStateChange(panelState: PanelStateMojo) {
    this.sender.requestNoResponse('panelStateChanged', {
      panelState: panelStateToClient(panelState),
    });
  }

  notifyPanelCanAttachChange(canAttach: boolean) {
    this.sender.requestNoResponse('canAttachStateChanged', {canAttach});
  }

  notifyGeminiEnterpriseSettingsChanged(
      settings: GeminiEnterpriseSettingsMojo|null): void {
    this.sender.requestNoResponse('notifyGeminiEnterpriseSettingsChanged', {
      settings: settings || undefined,
    });
  }

  notifyMicrophonePermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyMicrophonePermissionStateChanged', {
      enabled: enabled,
    });
  }

  stopMicrophone(): Promise<void> {
    return this.sender.requestWithResponse('stopMicrophone', undefined);
  }

  notifyLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyLocationPermissionStateChanged', {
      enabled: enabled,
    });
  }

  notifyTabContextPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyTabContextPermissionStateChanged', {
      enabled: enabled,
    });
  }

  notifyOsLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyOsLocationPermissionStateChanged', {
      enabled: enabled,
    });
  }

  notifyClosedCaptioningSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyClosedCaptioningSettingChanged', {
      enabled: enabled,
    });
  }

  notifyDefaultTabContextPermissionStateChanged(enabled: boolean) {
    this.sender.requestNoResponse(
        'notifyDefaultTabContextPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyActuationOnWebSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse('notifyActuationOnWebSettingChanged', {
      enabled: enabled,
    });
  }

  notifyFileUploadStateChanged(state: FileUploadPolicyState): void {
    this.sender.requestNoResponse('notifyFileUploadStateChanged', {
      state: fileUploadPolicyStateToClient(state),
    });
  }

  notifyZoomLevelChanged(zoomFactor: number): void {
    this.host.onZoomLevelChanged(zoomFactor);
  }

  notifyFocusedTabChanged(focusedTabData: (FocusedTabDataMojo)): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'notifyFocusedTabChanged', {
          focusedTabDataPrivate: focusedTabDataToClient(focusedTabData, extras),
        },
        extras.transfers);
  }

  notifyPanelActiveChange(panelActive: boolean): void {
    this.sender.requestNoResponse('notifyPanelActiveChanged', {panelActive});
    this.host.panelIsActive = panelActive;
  }

  notifyManualResizeChanged(resizing: boolean): void {
    this.sender.requestNoResponse('notifyManualResizeChanged', {resizing});
  }

  notifyBrowserIsOpenChanged(browserIsOpen: boolean): void {
    this.sender.requestNoResponse('browserIsOpenChanged', {browserIsOpen});
  }

  notifyInstanceActivationChanged(instanceIsActive: boolean): void {
    // This isn't forwarded to the actual web client yet, as it's currently
    // only needed for the responsiveness logic, which is here.
    this.host.setInstanceIsActive(instanceIsActive);
  }

  notifyOsHotkeyStateChanged(hotkey: string): void {
    this.sender.requestNoResponse('notifyOsHotkeyStateChanged', {hotkey});
  }

  notifyPinnedTabsChanged(tabData: TabDataMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'notifyPinnedTabsChanged',
        {tabData: tabData.map((x) => tabDataToPrivate(x, extras))},
        extras.transfers);
  }

  notifyPinnedTabDataChanged(tabData: TabDataMojo): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'notifyPinnedTabDataChanged',
        {tabData: tabDataToPrivate(tabData, extras)}, extras.transfers);
  }


  notifyPageMetadataChanged(tabId: number, metadata: PageMetadataMojo|null):
      void {
    this.sender.requestNoResponse('pageMetadataChanged', {
      tabId: idToClient(tabId),
      pageMetadata: pageMetadataToClient(metadata),
    });
  }

  notifyAdditionalContext(context: AdditionalContextMojo): void {
    const extras = new ResponseExtras();
    const clientContext = additionalContextToClient(context, extras);
    this.sender.requestNoResponse(
        'notifyAdditionalContext', {context: clientContext}, extras.transfers);
  }

  notifyActOnWebCapabilityChanged(canActOnWeb: boolean): void {
    this.sender.requestNoResponse(
        'notifyActOnWebCapabilityChanged', {canActOnWeb});
  }

  notifyOnboardingCompletedChanged(completed: boolean): void {
    this.sender.requestNoResponse('onboardingCompletedChanged', {completed});
  }

  notifyActorTaskListRowClicked(taskId: number): void {
    this.sender.requestNoResponse('notifyActorTaskListRowClicked', {taskId});
  }
}
