// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the browser, sending messages to the client.

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import type {ActorClientInterface, ActorTaskState as ActorTaskStateMojo, AdditionalContext as AdditionalContextMojo, ExperimentalTriggeringUpdatesHandlerRemote, FocusedTabData as FocusedTabDataMojo, GeminiEnterpriseSettings as GeminiEnterpriseSettingsMojo, InvokeOptions as InvokeOptionsMojo, OpenPanelInfo as OpenPanelInfoMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, SkillPreview as SkillPreviewMojo, TabData as TabDataMojo, WebClientInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import {enumToClient} from '../enum_conversions.js';
import type {ActorClient, WebClient} from '../request_types.js';
import {ResponseExtras} from '../transport/messaging.js';

import type {NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo} from './../../actor_webui.mojom-webui.js';
import {additionalContextToClient, focusedTabDataToClient, idToClient, invokeOptionsToClient, navigationConfirmationRequestToClient, navigationConfirmationResponseToMojo, pageMetadataToClient, panelOpeningDataToClient, panelStateToClient, selectAutofillSuggestionsDialogRequestToClient, selectAutofillSuggestionsDialogResponseToMojo, selectCredentialDialogRequestToClient, selectCredentialDialogResponseToMojo, tabDataToClient, timeDeltaFromClient, userConfirmationDialogRequestToClient, userConfirmationDialogResponseToMojo, webClientModeToMojo, zeroStateSuggestionsToClient} from './conversions.js';
import type {GatedSender} from './gated_sender.js';
import type {ApiHostEmbedder, GlicApiHost} from './glic_api_host.js';
import {PanelOpenState} from './types.js';

export class WebClientImpl implements WebClientInterface {
  private sender: GatedSender<WebClient>;
  private clientCreated = Promise.withResolvers<void>();

  constructor(private host: GlicApiHost, private embedder: ApiHostEmbedder) {
    this.sender = this.host.sender;
  }

  markCreated() {
    this.clientCreated.resolve();
  }

  async getExperimentalTriggeringUpdates(
      handler: ExperimentalTriggeringUpdatesHandlerRemote):
      Promise<{success: boolean}> {
    const id = this.host.addExperimentalTriggeringUpdatesHandler(handler);
    try {
      const result = await this.sender.requestWithResponse(
          'glicWebClientGetExperimentalTriggeringUpdates', {
            observationId: id,
          });
      if (!result.success) {
        this.host.deleteExperimentalTriggeringUpdatesHandler(id);
      }
      return {success: result.success};
    } catch (e) {
      this.host.deleteExperimentalTriggeringUpdatesHandler(id);
      throw e;
    }
  }

  async processNotifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    await this.clientCreated.promise;
    this.host.setWaitingOnPanelWillOpen(true);
    let result;
    try {
      result = await this.sender.requestWithResponse(
          'glicWebClientNotifyPanelWillOpen',
          {panelOpeningData: panelOpeningDataToClient(panelOpeningData)});
    } finally {
      this.host.setWaitingOnPanelWillOpen(false);
      this.host.panelOpenStateChanged(PanelOpenState.OPEN);
    }

    // The web client is ready to show, ensure the webview is
    // displayed.
    const canUserResize = result.openPanelInfo?.canUserResize ?? true;
    this.embedder.enableDragResize(canUserResize);
    this.embedder.webClientReady();

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
    return this.sender.requestWithResponse(
        'glicWebClientNotifyPanelWasClosed', undefined);
  }
  notifyPanelWasClosed(): Promise<void> {
    return this.processNotifyPanelWasClosed();
  }

  invoke(options: InvokeOptionsMojo): Promise<void> {
    const extras = new ResponseExtras();
    return this.sender.requestWithResponse(
        'glicWebClientInvoke', {
          options: invokeOptionsToClient(options, extras),
        },
        extras.transfers);
  }

  notifyPanelStateChange(panelState: PanelStateMojo) {
    this.sender.requestNoResponse('glicWebClientPanelStateChanged', {
      panelState: panelStateToClient(panelState),
    });
  }

  notifyPanelCanAttachChange(canAttach: boolean) {
    this.sender.requestNoResponse(
        'glicWebClientCanAttachStateChanged', {canAttach});
  }

  notifyGeminiEnterpriseSettingsChanged(
      settings: GeminiEnterpriseSettingsMojo|null): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyGeminiEnterpriseSettingsChanged', {
          settings: settings || undefined,
        });
  }

  notifyMicrophonePermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyMicrophonePermissionStateChanged', {
          enabled: enabled,
        });
  }

  stopMicrophone(): Promise<void> {
    return this.sender.requestWithResponse(
        'glicWebClientStopMicrophone', undefined);
  }

  notifyLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyLocationPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyTabContextPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyTabContextPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyOsLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyOsLocationPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyClosedCaptioningSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyClosedCaptioningSettingChanged', {
          enabled: enabled,
        });
  }

  notifyDefaultTabContextPermissionStateChanged(enabled: boolean) {
    this.sender.requestNoResponse(
        'glicWebClientNotifyDefaultTabContextPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyActuationOnWebSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyActuationOnWebSettingChanged', {
          enabled: enabled,
        });
  }

  notifyFocusedTabChanged(focusedTabData: (FocusedTabDataMojo)): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyFocusedTabChanged', {
          focusedTabDataPrivate: focusedTabDataToClient(focusedTabData, extras),
        },
        extras.transfers);
  }

  notifyPanelActiveChange(panelActive: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyPanelActiveChanged', {panelActive});
    this.host.panelIsActive = panelActive;
    this.host.updateSenderActive();
  }

  notifyIsInvoking(isInvoking: boolean): void {
    this.host.setIsInvoking(isInvoking);
  }

  notifyManualResizeChanged(resizing: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyManualResizeChanged', {resizing});
  }

  notifyBrowserIsOpenChanged(browserIsOpen: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientBrowserIsOpenChanged', {browserIsOpen});
  }

  notifyInstanceActivationChanged(instanceIsActive: boolean): void {
    // This isn't forwarded to the actual web client yet, as it's currently
    // only needed for the responsiveness logic, which is here.
    this.host.setInstanceIsActive(instanceIsActive);
  }

  notifyOsHotkeyStateChanged(hotkey: string): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyOsHotkeyStateChanged', {hotkey});
  }

  notifyPinnedTabsChanged(tabData: TabDataMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyPinnedTabsChanged',
        {tabData: tabData.map((x) => tabDataToClient(x, extras))},
        extras.transfers);
  }

  notifyPinnedTabDataChanged(tabData: TabDataMojo): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyPinnedTabDataChanged',
        {tabData: tabDataToClient(tabData, extras)}, extras.transfers,
        // Cache only one entry per tab ID.
        `${tabData.tabId}`);
  }

  notifySkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifySkillPreviewsChanged', {
          skillPreviews: skillPreviews.map(s => ({
                                             ...s,
                                             source: enumToClient(s.source),
                                             isContextual: false,
                                           })),
        });
  }

  notifyContextualSkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]):
      void {
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyContextualSkillPreviewsChanged', {
          contextualSkillPreviews:
              skillPreviews.map(s => ({
                                  ...s,
                                  source: enumToClient(s.source),
                                  isContextual: true,
                                })),
        });
  }

  notifySkillPreviewChanged(skillPreview: SkillPreviewMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifySkillPreviewChanged', {
          skillPreview: {
            ...skillPreview,
            source: enumToClient(skillPreview.source),
          },
        },
        [],
        // Cache only one entry per skill ID.
        `skill-${skillPreview.id}`);
  }

  notifySkillDeleted(skillId: string): void {
    this.sender.sendWhenActive('glicWebClientNotifySkillDeleted', {
      skillId,
    });
  }

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      options: ZeroStateSuggestionsOptionsMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientZeroStateSuggestionsChanged', {
          suggestions: zeroStateSuggestionsToClient(suggestions),
          options: options,
        });
  }

  notifyPageMetadataChanged(tabId: number, metadata: PageMetadataMojo|null):
      void {
    this.sender.sendLatestWhenActive(
        'glicWebClientPageMetadataChanged', {
          tabId: idToClient(tabId),
          pageMetadata: pageMetadataToClient(metadata),
        },
        undefined, `${tabId}`);
  }

  notifyAdditionalContext(context: AdditionalContextMojo): void {
    const extras = new ResponseExtras();
    const clientContext = additionalContextToClient(context, extras);
    this.sender.sendWhenActive(
        'glicWebClientNotifyAdditionalContext', {context: clientContext},
        extras.transfers);
  }

  notifyActOnWebCapabilityChanged(canActOnWeb: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyActOnWebCapabilityChanged', {canActOnWeb});
  }

  notifyOnboardingCompletedChanged(completed: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientOnboardingCompletedChanged', {completed});
  }

  notifyActorTaskListRowClicked(taskId: number): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyActorTaskListRowClicked', {taskId});
  }
}

export class ActorClientImpl implements ActorClientInterface {
  constructor(private sender: GatedSender<ActorClient>) {}

  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    const clientState = enumToClient(state);
    this.sender.requestNoResponse(
        'glicWebClientNotifyActorTaskStateChanged',
        {taskId, state: clientState});
  }

  async requestToShowCredentialSelectionDialog(
      request: SelectCredentialDialogRequestMojo):
      Promise<{response: SelectCredentialDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToShowDialog',
        {request: selectCredentialDialogRequestToClient(request)});
    return {
      response: selectCredentialDialogResponseToMojo(clientResponse.response),
    };
  }

  async requestToShowUserConfirmationDialog(
      request: UserConfirmationDialogRequestMojo):
      Promise<{response: UserConfirmationDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToShowConfirmationDialog',
        {request: userConfirmationDialogRequestToClient(request)});
    return {
      response: userConfirmationDialogResponseToMojo(clientResponse.response),
    };
  }

  async requestToConfirmNavigation(request: NavigationConfirmationRequestMojo):
      Promise<{response: NavigationConfirmationResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToConfirmNavigation',
        {request: navigationConfirmationRequestToClient(request)});
    return {
      response: navigationConfirmationResponseToMojo(clientResponse.response),
    };
  }

  async requestToShowAutofillSuggestionsDialog(
      request: SelectAutofillSuggestionsDialogRequestMojo):
      Promise<{response: SelectAutofillSuggestionsDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToShowAutofillSuggestionsDialog',
        {request: selectAutofillSuggestionsDialogRequestToClient(request)});
    return {
      response: selectAutofillSuggestionsDialogResponseToMojo(
          clientResponse.response),
    };
  }
}
