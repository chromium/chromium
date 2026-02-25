// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for converting from mojom types to message types.
// Summary of changes:
// * Window and tab IDs are sent using int32 in mojo, but made opaque
//   strings for the public API. This allows Chrome to change the ID
//   representation later.
// * Optional types in Mojo use null, but optional types in the public API use
//   undefined.

import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {AlphaType} from '//resources/mojo/skia/public/mojom/image_info.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {PageMetadata as PageMetadataMojo} from '../../ai_page_content_metadata.mojom-webui.js';
import type {AdditionalContext as AdditionalContextMojo, AdditionalContextPart as AdditionalContextPartMojo, AnnotatedPageData as AnnotatedPageDataMojo, CaptureRegionResult as CaptureRegionResultMojo, ContextData as ContextDataMojo, ConversationInfo as ConversationInfoMojo, FocusedTabData as FocusedTabDataMojo, FormFactor as FormFactorMojo, GetPinCandidatesOptions as GetPinCandidatesOptionsMojo, GetTabContextOptions as TabContextOptionsMojo, HostCapability as HostCapabilityMojo, InvokeOptions as InvokeOptionsMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, PdfDocumentData as PdfDocumentDataMojo, PinTabsOptions as PinTabsOptionsMojo, Platform as PlatformMojo, Screenshot as ScreenshotMojo, TabContext as TabContextMojo, TabData as TabDataMojo, UnpinTabsOptions as UnpinTabsOptionsMojo, WebPageData as WebPageDataMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import {MicrophoneStatus as MicrophoneStatusMojo, PinTrigger as PinTriggerMojo, UnpinTrigger as UnpinTriggerMojo, WebClientMode as WebClientModeMojo} from '../../glic.mojom-webui.js';
import type {AdditionalContextSource, CaptureRegionResult, ConversationInfo, CredentialType, FeatureMode, FormFactor, GetPinCandidatesOptions, HostCapability, InvocationSource, PageMetadata, PanelOpeningData, PanelState, PinTabsOptions, PinTrigger, Platform, Screenshot, TabContextOptions, TaskOptions, UnpinTabsOptions, UnpinTrigger, WebPageData, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {DEFAULT_INNER_TEXT_BYTES_LIMIT, DEFAULT_PDF_SIZE_LIMIT, MicrophoneStatus, WebClientMode} from '../../glic_api/glic_api.js';

import type {ConfirmationRequestErrorReason as ConfirmationRequestErrorReasonMojo, CredentialType as CredentialTypeMojo, NavigationConfirmationRequest as NavigationConfirmationRequestMojo, NavigationConfirmationResponse as NavigationConfirmationResponseMojo, SelectAutofillSuggestionsDialogErrorReason as SelectAutofillSuggestionsDialogErrorReasonMojo, SelectAutofillSuggestionsDialogRequest as SelectAutofillSuggestionsDialogRequestMojo, SelectAutofillSuggestionsDialogResponse as SelectAutofillSuggestionsDialogResponseMojo, SelectCredentialDialogErrorReason as SelectCredentialDialogErrorReasonMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, TaskOptions as TaskOptionsMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo, UserGrantedPermissionDuration as UserGrantedPermissionDurationMojo} from './../../actor_webui.mojom-webui.js';
import {replaceProperties} from './../conversions.js';
import type {ResponseExtras} from './../post_message_transport.js';
import type {AdditionalContextPartPrivate, AdditionalContextPrivate, AnnotatedPageDataPrivate, FocusedTabDataPrivate, InvokeOptionsPrivate, NavigationConfirmationRequestPrivate, NavigationConfirmationResponsePrivate, PdfDocumentDataPrivate, ResumeActorTaskResultPrivate, RgbaImage, SelectAutofillSuggestionsDialogRequestPrivate, SelectAutofillSuggestionsDialogResponsePrivate, SelectCredentialDialogRequestPrivate, SelectCredentialDialogResponsePrivate, TabContextResultPrivate, TabDataPrivate, UserConfirmationDialogRequestPrivate, UserConfirmationDialogResponsePrivate} from './../request_types.js';
import {ImageAlphaType, ImageColorType} from './../request_types.js';


export function idToClient(windowId: number): string;
export function idToClient(windowId: number|null): string|undefined;
export function idToClient(windowId: number|null): string|undefined {
  if (windowId === null) {
    return undefined;
  }

  if (Number.isNaN(windowId)) {
    return '0';
  }

  return `${windowId}`;
}

export function idFromClient(windowId: string): number;
export function idFromClient(windowId: string|undefined): number|null;
export function idFromClient(windowId: string|undefined): number|null {
  if (windowId === undefined) {
    return null;
  }

  const parsed = parseInt(windowId);
  if (Number.isNaN(parsed)) {
    return 0;
  }

  return parsed;
}

export function screenshotToClient(
    screenshot: ScreenshotMojo|null, extras: ResponseExtras): Screenshot|
    undefined {
  if (!screenshot) {
    return undefined;
  }
  const screenshotArray = new Uint8Array(screenshot.data);
  const buffer = screenshotArray.buffer;
  extras.addTransfer(buffer);
  return {
    widthPixels: screenshot.widthPixels,
    heightPixels: screenshot.heightPixels,
    data: buffer,
    mimeType: screenshot.mimeType,
    originAnnotations: {},
  };
}

export function contextDataToClient(
    data: ContextDataMojo,
    extras: ResponseExtras): {mimeType: string, data: ArrayBuffer}|undefined {
  const buffer = getArrayBufferFromBigBuffer(data.data);
  if (!buffer) {
    return undefined;
  }
  extras.addTransfer(buffer);
  return {mimeType: data.mimeType, data: buffer};
}

export function webPageDataToClient(webPageData: WebPageDataMojo|null):
    WebPageData|undefined {
  if (!webPageData) {
    return undefined;
  }
  return {
    mainDocument: {
      origin: originToClient(webPageData.mainDocument.origin),
      innerText: webPageData.mainDocument.innerText,
      innerTextTruncated: webPageData.mainDocument.innerTextTruncated,
    },
  };
}

export function pdfDocumentDataToClient(
    pdfDocumentData: PdfDocumentDataMojo|null,
    extras: ResponseExtras): PdfDocumentDataPrivate|undefined {
  if (!pdfDocumentData) {
    return undefined;
  }
  const pdfData = pdfDocumentData.pdfData ?
      new Uint8Array(pdfDocumentData.pdfData).buffer :
      undefined;
  if (pdfData) {
    extras.addTransfer(pdfData);
  }
  return {
    origin: originToClient(pdfDocumentData.origin),
    pdfSizeLimitExceeded: pdfDocumentData.sizeLimitExceeded,
    pdfData,
  };
}

export function annotatedPageDataToClient(
    annotatedPageData: AnnotatedPageDataMojo|null,
    extras: ResponseExtras): AnnotatedPageDataPrivate|undefined {
  if (!annotatedPageData) {
    return undefined;
  }
  const annotatedPageContent = annotatedPageData.annotatedPageContent ?
      getArrayBufferFromBigBuffer(
          annotatedPageData.annotatedPageContent.smuggled) :
      undefined;
  if (annotatedPageContent) {
    extras.addTransfer(annotatedPageContent);
  }
  let metadata: PageMetadata|undefined = undefined;
  if (annotatedPageData.metadata) {
    metadata = {
      frameMetadata: annotatedPageData.metadata.frameMetadata.map(
          m => replaceProperties(m, {url: urlToClient(m.url)})),
    };
  }
  return {annotatedPageContent, metadata};
}

export function optionalToClient<T>(value: T|null) {
  if (value === null) {
    return undefined;
  }
  return value;
}

export function optionalFromClient<T>(value: T|undefined) {
  if (value === undefined) {
    return null;
  }
  return value;
}

export function urlToClient(url: Url): string;
export function urlToClient(url: Url|null): string|undefined;
export function urlToClient(url: Url|null): string|undefined {
  if (url === null) {
    return undefined;
  }
  return url;
}

export function urlFromClient(url: string): Url {
  return url;
}

export function originToClient(origin: Origin): string;
export function originToClient(origin: Origin|null): string|undefined;
export function originToClient(origin: Origin|null): string|undefined {
  if (!origin) {
    return undefined;
  }
  if (!origin.scheme) {
    return '';
  }
  const originBase = `${origin.scheme}://${origin.host}`;
  if (origin.port) {
    return `${originBase}:${origin.port}`;
  }
  return originBase;
}

export function tabDataToClient(
    tabData: TabDataMojo, extras: ResponseExtras): TabDataPrivate;
export function tabDataToClient(
    tabData: TabDataMojo|null, extras: ResponseExtras): TabDataPrivate|
    undefined;
export function tabDataToClient(
    tabData: TabDataMojo|null, extras: ResponseExtras): TabDataPrivate|
    undefined {
  if (!tabData) {
    return undefined;
  }

  let favicon: RgbaImage|undefined = undefined;
  if (tabData.favicon) {
    favicon = bitmapN32ToRGBAImage(tabData.favicon);
    if (favicon) {
      extras.addTransfer(favicon.dataRGBA);
    }
  }

  const isObservable = optionalToClient(tabData.isObservable);
  const isMediaActive = optionalToClient(tabData.isMediaActive);
  const isTabContentCaptured = optionalToClient(tabData.isTabContentCaptured);
  const isActiveInWindow = optionalToClient(tabData.isActiveInWindow);
  const isWindowActive = optionalToClient(tabData.isWindowActive);
  return {
    tabId: idToClient(tabData.tabId),
    windowId: idToClient(tabData.windowId),
    url: urlToClient(tabData.url),
    title: optionalToClient(tabData.title),
    favicon,
    faviconUrl: urlToClient(tabData.faviconUrl),
    documentMimeType: tabData.documentMimeType,
    isObservable,
    isMediaActive,
    isTabContentCaptured,
    isActiveInWindow,
    isWindowActive,
  };
}

export function focusedTabDataToClient(
    focusedTabData: FocusedTabDataMojo,
    extras: ResponseExtras): FocusedTabDataPrivate {
  if (focusedTabData.focusedTab) {
    return {
      hasFocus: {tabData: tabDataToClient(focusedTabData.focusedTab, extras)},
    };
  }
  if (focusedTabData.noFocusedTabData) {
    return {
      hasNoFocus: {
        tabFocusCandidateData: tabDataToClient(
            focusedTabData.noFocusedTabData.activeTabData, extras),
        noFocusReason: focusedTabData.noFocusedTabData.noFocusReason,
      },
    };
  }
  console.error('Invalid FocusedTabDataMojo');
  return {};
}

export function getArrayBufferFromBigBuffer(bigBuffer: BigBuffer): ArrayBuffer|
    undefined {
  if (bigBuffer.bytes !== undefined) {
    return new Uint8Array(bigBuffer.bytes).buffer;
  }
  return bigBuffer.sharedMemory?.bufferHandle
      .mapBuffer(0, bigBuffer.sharedMemory.size)
      .buffer;
}

export function bitmapN32ToRGBAImage(bitmap: BitmapN32): RgbaImage|undefined {
  const bytes = getArrayBufferFromBigBuffer(bitmap.pixelData);
  if (!bytes) {
    return undefined;
  }
  // We don't transmit ColorType over mojo, because it's determined by the
  // endianness of the platform. Chromium only supports little endian, which
  // maps to BGRA. See third_party/skia/include/core/SkColorType.h.
  const colorType = ImageColorType.BGRA;

  return {
    width: bitmap.imageInfo.width,
    height: bitmap.imageInfo.height,
    dataRGBA: bytes,
    alphaType: bitmap.imageInfo.alphaType === AlphaType.PREMUL ?
        ImageAlphaType.PREMUL :
        ImageAlphaType.UNPREMUL,
    colorType,
  };
}

export function panelOpeningDataToClient(
    panelOpeningData: PanelOpeningDataMojo): PanelOpeningData {
  const conversationInfo =
      conversationInfoToClient(panelOpeningData.conversationInfo);
  return {
    panelState: panelStateToClient(panelOpeningData.panelState),
    invocationSource: panelOpeningData.invocationSource as number,
    conversationId: conversationInfo?.conversationId || undefined,
    promptSuggestion: optionalToClient(panelOpeningData.promptSuggestion),
    autoSend: panelOpeningData.autoSend && !!panelOpeningData.promptSuggestion,
    recentlyActiveConversations: panelOpeningData.recentlyActiveConversations ?
        panelOpeningData.recentlyActiveConversations.map(
            conversationInfoToClient) :
        undefined,
    conversationInfo,
  };
}

export function conversationInfoToClient(
    conversationInfo: ConversationInfoMojo): ConversationInfo {
  return {
    conversationId: conversationInfo.conversationId,
    conversationTitle: conversationInfo.conversationTitle,
    clientData: conversationInfo.clientData ?
        new TextDecoder().decode(
            new Uint8Array(conversationInfo.clientData.data)) :
        undefined,
  };
}

export function conversationInfoFromClient(conversationInfo: ConversationInfo):
    ConversationInfoMojo {
  return {
    conversationId: conversationInfo.conversationId,
    conversationTitle: conversationInfo.conversationTitle,
    clientData: conversationInfo.clientData ? {
      data: Array.from(new TextEncoder().encode(conversationInfo.clientData)),
    } :
                                              null,
  };
}

export function panelStateToClient(panelState: PanelStateMojo): PanelState {
  return {
    kind: panelState.kind as number,
    windowId: idToClient(panelState.windowId),
  };
}

export function pageMetadataToClient(metadata: PageMetadataMojo|null):
    PageMetadata|null {
  if (!metadata) {
    return null;
  }
  return {
    frameMetadata: metadata.frameMetadata.map(
        m => replaceProperties(m, {url: urlToClient(m.url)})),
  };
}

/** Takes a time value in milliseconds and converts to a Mojo TimeDelta. */
export function timeDeltaFromClient(durationMs: number = 0): TimeDelta {
  if (!Number.isFinite(durationMs)) {
    throw new Error('Invalid duration value: ' + durationMs);
  }
  return {microseconds: BigInt(Math.floor(durationMs * 1000))};
}

export function tabContextToClient(
    tabContext: TabContextMojo,
    extras: ResponseExtras): TabContextResultPrivate {
  const tabData: TabDataPrivate = tabDataToClient(tabContext.tabData, extras);
  const webPageData = webPageDataToClient(tabContext.webPageData);
  const viewportScreenshot =
      screenshotToClient(tabContext.viewportScreenshot, extras);
  const pdfDocumentData =
      pdfDocumentDataToClient(tabContext.pdfDocumentData, extras);
  const annotatedPageData =
      annotatedPageDataToClient(tabContext.annotatedPageData, extras);

  return {
    tabData,
    webPageData,
    viewportScreenshot,
    pdfDocumentData,
    annotatedPageData,
  };
}

export function resumeActorTaskResultToClient(
    tabContext: TabContextMojo, actionResult: number,
    extras: ResponseExtras): ResumeActorTaskResultPrivate {
  const tabData: TabDataPrivate = tabDataToClient(tabContext.tabData, extras);
  const webPageData = webPageDataToClient(tabContext.webPageData);
  const viewportScreenshot =
      screenshotToClient(tabContext.viewportScreenshot, extras);
  const pdfDocumentData =
      pdfDocumentDataToClient(tabContext.pdfDocumentData, extras);
  const annotatedPageData =
      annotatedPageDataToClient(tabContext.annotatedPageData, extras);

  return {
    tabData,
    webPageData,
    viewportScreenshot,
    pdfDocumentData,
    annotatedPageData,
    actionResult,
  };
}

export function tabContextOptionsFromClient(options: TabContextOptions):
    TabContextOptionsMojo {
  return {
    includeInnerText: options.innerText ?? false,
    innerTextBytesLimit:
        options.innerTextBytesLimit ?? DEFAULT_INNER_TEXT_BYTES_LIMIT,
    includeViewportScreenshot: options.viewportScreenshot ?? false,
    includePdf: options.pdfData ?? false,
    includeAnnotatedPageContent: options.annotatedPageContent ?? false,
    maxMetaTags: options.maxMetaTags ?? 0,
    pdfSizeLimit: options.pdfSizeLimit === undefined ?
        DEFAULT_PDF_SIZE_LIMIT :
        Math.min(Number.MAX_SAFE_INTEGER, options.pdfSizeLimit),
    annotatedPageContentMode: options.annotatedPageContentMode === undefined ?
        0 :
        options.annotatedPageContentMode,
  };
}

// Taken from mojo_type_utils.ts
export function getPinCandidatesOptionsFromClient(
    options: GetPinCandidatesOptions): GetPinCandidatesOptionsMojo {
  return {
    maxCandidates: options.maxCandidates,
    query: options.query ?? null,
  };
}

export function pinTriggerToMojo(trigger: PinTrigger|undefined):
    PinTriggerMojo {
  return (trigger ?? PinTriggerMojo.kWebClientUnknown) as PinTriggerMojo;
}

export function pinTabsOptionsToMojo(options: PinTabsOptions|undefined):
    PinTabsOptionsMojo|null {
  if (!options) {
    return null;
  }
  return {
    pinTrigger: pinTriggerToMojo(options.pinTrigger),
  };
}

export function unpinTriggerToMojo(trigger: UnpinTrigger|undefined):
    UnpinTriggerMojo {
  return (trigger ?? UnpinTriggerMojo.kWebClientUnknown) as UnpinTriggerMojo;
}

export function unpinTabsOptionsToMojo(options: UnpinTabsOptions|undefined):
    UnpinTabsOptionsMojo|null {
  if (!options) {
    return null;
  }
  return {
    unpinTrigger: unpinTriggerToMojo(options.unpinTrigger),
  };
}

export function byteArrayFromClient(buffer: ArrayBuffer): number[] {
  const byteArray = new Uint8Array(buffer);
  return Array.from(byteArray);
}

export function hostCapabilitiesToClient(capabilities: HostCapabilityMojo[]):
    HostCapability[] {
  return capabilities.map(capability => capability as number as HostCapability);
}

export function platformToClient(platform: PlatformMojo): Platform {
  return platform as number as Platform;
}

export function formFactorToClient(formFactor: FormFactorMojo): FormFactor {
  return formFactor as number as FormFactor;
}

export function selectCredentialDialogResponseToMojo(
    response: SelectCredentialDialogResponsePrivate):
    SelectCredentialDialogResponseMojo {
  return response.errorReason ?
      {
        taskId: response.taskId,
        errorReason: response.errorReason as number as
            SelectCredentialDialogErrorReasonMojo,
        permissionDuration: null,
        selectedCredentialId: null,
      } :
      {
        ...response,
        errorReason: null,
        permissionDuration: optionalFromClient(response.permissionDuration) as
                UserGrantedPermissionDurationMojo |
            null,
        selectedCredentialId: response.selectedCredentialId ?? null,
      };
}

function credentialTypeToClient(credentialType: CredentialTypeMojo):
    CredentialType {
  return credentialType as number as CredentialType;
}

export function selectCredentialDialogRequestToClient(
    request: SelectCredentialDialogRequestMojo):
    SelectCredentialDialogRequestPrivate {
  const icons = new Map<string, RgbaImage>();
  if (request.icons) {
    for (const [siteOrApp, value] of Object.entries(request.icons)) {
      const rgbaImage = bitmapN32ToRGBAImage(value);
      if (rgbaImage) {
        icons.set(siteOrApp, rgbaImage);
      }
    }
  }
  return {
    ...request,
    credentials: request.credentials.map(
        credential => ({
          ...credential,
          requestOrigin: originToClient(credential.requestOrigin),
          type: credentialTypeToClient(credential.type),
        })),
    icons,
  };
}

export function userConfirmationDialogRequestToClient(
    request: UserConfirmationDialogRequestMojo):
    UserConfirmationDialogRequestPrivate {
  return {
    navigationOrigin: request.payload.navigationOrigin ?
        originToClient(request.payload.navigationOrigin) :
        undefined,
    forBlocklistedOrigin: request.payload.forBlocklistedOrigin,
  };
}

export function userConfirmationDialogResponseToMojo(
    response: UserConfirmationDialogResponsePrivate):
    UserConfirmationDialogResponseMojo {
  if (response.errorReason) {
    return {
      result: {
        errorReason: response.errorReason as number as
            ConfirmationRequestErrorReasonMojo,
      },
    };
  }
  return {
    result: {permissionGranted: response.permissionGranted},
  };
}

export function navigationConfirmationRequestToClient(
    request: NavigationConfirmationRequestMojo):
    NavigationConfirmationRequestPrivate {
  return {
    taskId: request.taskId,
    navigationOrigin: originToClient(request.navigationOrigin),
  };
}

export function navigationConfirmationResponseToMojo(
    response: NavigationConfirmationResponsePrivate):
    NavigationConfirmationResponseMojo {
  if (response.errorReason) {
    return {
      result: {
        errorReason: response.errorReason as number as
            ConfirmationRequestErrorReasonMojo,
      },
    };
  }
  return {
    result: {
      permissionGranted: response.permissionGranted,
    },
  };
}

export function selectAutofillSuggestionsDialogRequestToClient(
    request: SelectAutofillSuggestionsDialogRequestMojo):
    SelectAutofillSuggestionsDialogRequestPrivate {
  return {
    ...request,
    formFillingRequests: request.formFillingRequests.map(
        r => ({
          ...r,
          requestedData: Number(r.requestedData),
          suggestions: r.suggestions.map(
              s => ({
                ...s,
                icon: s.icon ? bitmapN32ToRGBAImage(s.icon) : undefined,
              })),
        })),
  };
}

export function selectAutofillSuggestionsDialogResponseToMojo(
    response: SelectAutofillSuggestionsDialogResponsePrivate):
    SelectAutofillSuggestionsDialogResponseMojo {
  if (response.errorReason) {
    return {
      taskId: response.taskId,
      result: {
        errorReason: response.errorReason as number as
            SelectAutofillSuggestionsDialogErrorReasonMojo,
      },
    };
  } else {
    return {
      taskId: response.taskId,
      result: {
        selectedSuggestions: response.selectedSuggestions,
      },
    };
  }
}

export function taskOptionsToMojo(taskOptions?: TaskOptions): TaskOptionsMojo|
    null {
  if (taskOptions) {
    return {
      title: taskOptions.title ?? null,
    };
  }
  return null;
}

export function webClientModeToMojo(mode: WebClientMode|undefined):
    WebClientModeMojo {
  switch (mode) {
    case WebClientMode.AUDIO:
      return WebClientModeMojo.kAudio;
    case WebClientMode.TEXT:
      return WebClientModeMojo.kText;
    default:
      return WebClientModeMojo.kUnknown;
  }
}

export function microphoneStatusToMojo(status: MicrophoneStatus):
    MicrophoneStatusMojo {
  switch (status) {
    case MicrophoneStatus.LISTENING:
      return MicrophoneStatusMojo.kListening;
    case MicrophoneStatus.NOT_LISTENING:
      return MicrophoneStatusMojo.kNotListening;
    case MicrophoneStatus.UNKNOWN:
    default:
      return MicrophoneStatusMojo.kUnknown;
  }
}

export function captureRegionResultToClient(
    result: CaptureRegionResultMojo|null): CaptureRegionResult|undefined {
  if (!result) {
    return undefined;
  }
  const region = result.region.rect ? {rect: result.region.rect} : undefined;
  return {
    tabId: idToClient(result.tabId),
    region,
  };
}

export function zeroStateSuggestionsToClient(
    zeroStateSuggestions: ZeroStateSuggestionsV2Mojo): ZeroStateSuggestionsV2 {
  return {
    suggestions: zeroStateSuggestions.suggestions,
    isPending: zeroStateSuggestions.isPending,
    invocationSource: zeroStateSuggestions.invocationSource as number as
        InvocationSource,
  };
}

export function invokeOptionsToClient(
    options: InvokeOptionsMojo, extras: ResponseExtras): InvokeOptionsPrivate {
  return {
    invocationSource: options.invocationSource as number as InvocationSource,
    prompts: options.prompts || undefined,
    context: options.context ?
        additionalContextToClient(options.context, extras) :
        undefined,
    autoSubmit: options.autoSubmit,
    featureMode: options.featureMode as number as FeatureMode,
    disableZeroStateSuggestions: options.disableZeroStateSuggestions,
    skillId: options.skillId || undefined,
  };
}

export function additionalContextToClient(
    context: AdditionalContextMojo,
    extras: ResponseExtras): AdditionalContextPrivate {
  const parts = context.parts ?
      context.parts.map(p => additionalContextPartToClient(p, extras)) :
      [];
  // Filter out undefined parts if any.
  const validParts: AdditionalContextPartPrivate[] = [];
  for (const p of parts) {
    if (p) {
      validParts.push(p);
    }
  }

  return {
    source: context.source as number as AdditionalContextSource,
    name: context.name || undefined,
    tabId: idToClient(context.tabId),
    origin: context.origin ? originToClient(context.origin) : undefined,
    frameUrl: context.frameUrl ? urlToClient(context.frameUrl) : undefined,
    parts: validParts,
  };
}

export function additionalContextPartToClient(
    part: AdditionalContextPartMojo,
    extras: ResponseExtras): AdditionalContextPartPrivate|undefined {
  const result: AdditionalContextPartPrivate = {};
  if (part.data) {
    result.data = {
      mimeType: part.data.mimeType,
      data: getArrayBufferFromBigBuffer(part.data.data)!,
    };
    extras.addTransfer(result.data.data);
  } else if (part.screenshot) {
    result.screenshot = screenshotToClient(part.screenshot, extras);
  } else if (part.webPageData) {
    result.webPageData = webPageDataToClient(part.webPageData);
  } else if (part.annotatedPageData) {
    result.annotatedPageData =
        annotatedPageDataToClient(part.annotatedPageData, extras);
  } else if (part.pdfDocumentData) {
    result.pdf = pdfDocumentDataToClient(part.pdfDocumentData, extras);
  } else if (part.tabContext) {
    result.tabContext = tabContextToClient(part.tabContext, extras);
  } else if (part.region) {
    const regionMojo = part.region;
    if (regionMojo) {
      return {
        region: {
          rect: regionMojo.rect,
        },
      };
    }
  }
  return result;
}
