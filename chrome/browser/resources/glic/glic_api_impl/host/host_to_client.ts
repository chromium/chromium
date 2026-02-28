// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the browser, sending messages to the client.

import {loadTimeData} from '//resources/js/load_time_data.js';

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import type {ActorTaskState as ActorTaskStateMojo, AdditionalContext as AdditionalContextMojo, FocusedTabData as FocusedTabDataMojo, InvokeOptions as InvokeOptionsMojo, OpenPanelInfo as OpenPanelInfoMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, Skill as SkillMojo, SkillPreview as SkillPreviewMojo, TabData as TabDataMojo, WebClientInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import type * as api from '../../glic_api/glic_api.js';
import type {SkillSource} from '../../glic_api/glic_api.js';

import type {NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo} from './../../actor_webui.mojom-webui.js';
import {ResponseExtras} from './../post_message_transport.js';
import {additionalContextToClient, focusedTabDataToClient, idToClient, invokeOptionsToClient, navigationConfirmationRequestToClient, navigationConfirmationResponseToMojo, optionalToClient, pageMetadataToClient, panelOpeningDataToClient, panelStateToClient, selectAutofillSuggestionsDialogRequestToClient, selectAutofillSuggestionsDialogResponseToMojo, selectCredentialDialogRequestToClient, selectCredentialDialogResponseToMojo, tabDataToClient, timeDeltaFromClient, userConfirmationDialogRequestToClient, userConfirmationDialogResponseToMojo, webClientModeToMojo, zeroStateSuggestionsToClient} from './conversions.js';
import type {GatedSender} from './gated_sender.js';
import type {ApiHostEmbedder, GlicApiHost} from './glic_api_host.js';
import {PanelOpenState} from './types.js';

export class WebClientImpl implements WebClientInterface {
  private sender: GatedSender;

  constructor(private host: GlicApiHost, private embedder: ApiHostEmbedder) {
    this.sender = this.host.sender;
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
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
    if (!loadTimeData.getBoolean('glicWebContentsWarming')) {
      this.embedder.webClientReady();
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
      this.embedder.onGuestResizeRequest(size);
      openPanelInfoMojo.panelSize = size;
    }
    return {openPanelInfo: openPanelInfoMojo};
  }

  notifyPanelWasClosed(): Promise<void> {
    this.host.panelOpenStateChanged(PanelOpenState.CLOSED);
    return this.sender.requestWithResponse(
        'glicWebClientNotifyPanelWasClosed', undefined);
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
          skillPreviews:
              skillPreviews.map(s => ({
                                  ...s,
                                  source: s.source as number as SkillSource,
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
                                  source: s.source as number as SkillSource,
                                  isContextual: true,
                                })),
        });
  }

  notifySkillPreviewChanged(skillPreview: SkillPreviewMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifySkillPreviewChanged', {
          skillPreview: {
            ...skillPreview,
            source: skillPreview.source as number as SkillSource,
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

  notifySkillToInvokeChanged(skill: SkillMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifySkillToInvokeChanged', {
          skill: {
            ...skill,
            preview: {
              ...skill.preview,
              source: skill.preview.source as number as SkillSource,
            },
            sourceSkillId: optionalToClient(skill.sourceSkillId),
          },
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

  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    const clientState = state as number as api.ActorTaskState;
    this.sender.requestNoResponse(
        'glicWebClientNotifyActorTaskStateChanged',
        {taskId, state: clientState});
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
