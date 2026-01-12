// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles messages from the browser, sending messages to the client.

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import type {ActorTaskState as ActorTaskStateMojo, AdditionalContext as AdditionalContextMojo, FocusedTabData as FocusedTabDataMojo, OpenPanelInfo as OpenPanelInfoMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, TabData as TabDataMojo, ViewChangeRequest as ViewChangeRequestMojo, WebClientInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import type * as api from '../../glic_api/glic_api.js';
import type {ViewChangeRequest} from '../../glic_api/glic_api.js';
import {ClientView} from '../../glic_api/glic_api.js';

import type {NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo} from './../../actor_webui.mojom-webui.js';
import {ResponseExtras} from './../post_message_transport.js';
import type {AdditionalContextPartPrivate, AdditionalContextPrivate} from './../request_types.js';
import {annotatedPageDataToClient, contextDataToClient, focusedTabDataToClient, idToClient, navigationConfirmationRequestToClient, navigationConfirmationResponseToMojo, optionalToClient, originToClient, pageMetadataToClient, panelOpeningDataToClient, panelStateToClient, pdfDocumentDataToClient, screenshotToClient, selectAutofillSuggestionsDialogRequestToClient, selectAutofillSuggestionsDialogResponseToMojo, selectCredentialDialogRequestToClient, selectCredentialDialogResponseToMojo, tabContextToClient, tabDataToClient, timeDeltaFromClient, urlToClient, userConfirmationDialogRequestToClient, userConfirmationDialogResponseToMojo, webClientModeToMojo, webPageDataToClient} from './conversions.js';
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

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      options: ZeroStateSuggestionsOptionsMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientZeroStateSuggestionsChanged',
        {suggestions: suggestions, options: options});
  }

  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    const clientState = state as number as api.ActorTaskState;
    this.sender.requestNoResponse(
        'glicWebClientNotifyActorTaskStateChanged',
        {taskId, state: clientState});
  }

  notifyTabDataChanged(tabData: TabDataMojo): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'glicWebClientNotifyTabDataChanged', {
          tabData: tabDataToClient(tabData, extras),
        },
        extras.transfers);
  }

  requestViewChange(requestMojo: ViewChangeRequestMojo): void {
    let request: ViewChangeRequest|undefined;
    if (requestMojo.details.actuation) {
      request = {desiredView: ClientView.ACTUATION};
    } else if (requestMojo.details.conversation) {
      request = {desiredView: ClientView.CONVERSATION};
    }
    if (!request) {
      return;
    }
    this.sender.requestNoResponse('glicWebClientRequestViewChange', {request});
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
    const clientParts = context.parts.map(p => {
      const part: AdditionalContextPartPrivate = {};
      if (p.data) {
        part.data = contextDataToClient(p.data, extras);
      } else if (p.screenshot) {
        part.screenshot = screenshotToClient(p.screenshot, extras);
      } else if (p.webPageData) {
        part.webPageData = webPageDataToClient(p.webPageData);
      } else if (p.annotatedPageData) {
        part.annotatedPageData =
            annotatedPageDataToClient(p.annotatedPageData, extras);
      } else if (p.pdfDocumentData) {
        part.pdf = pdfDocumentDataToClient(p.pdfDocumentData, extras);
      } else if (p.tabContext) {
        part.tabContext = tabContextToClient(p.tabContext, extras);
      }
      return part;
    });

    const clientContext: AdditionalContextPrivate = {
      name: optionalToClient(context.name),
      tabId: idToClient(context.tabId),
      origin: originToClient(context.origin),
      frameUrl: urlToClient(context.frameUrl),
      parts: clientParts,
    };

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
