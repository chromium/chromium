// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for converting between mojom types and client API types.
// Summary of changes:
// * Window and tab IDs are sent using int32 in mojo, but made opaque
//   strings for the public API. This allows Chrome to change the ID
//   representation later.
// * Optional types in Mojo use null, but optional types in the public API use
//   undefined.

import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {ProtoWrapper} from '//resources/mojo/mojo/public/mojom/base/proto_wrapper.mojom-webui.js';
import type {Time, TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {AlphaType} from '//resources/mojo/skia/public/mojom/image_info.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {PageMetadata as PageMetadataMojo} from '../ai_page_content_metadata.mojom-webui.js';
import {enumFromClient, enumToClient} from '../enum_conversions.js';
import type {                                                                //
             AdditionalContext as AdditionalContextMojo,                     //
             AdditionalContextPart as AdditionalContextPartMojo,             //
             AnnotatedPageData as AnnotatedPageDataMojo,                     //
             CaptureRegionResult as CaptureRegionResultMojo,                 //
             ConversationInfo as ConversationInfoMojo,                       //
             CounterAbuseVerdict as CounterAbuseVerdictMojo,                 //
             CreateTabOptions as CreateTabOptionsMojo,                       //
             FileUploadPolicyState as FileUploadPolicyStateMojo,             //
             FocusedTabData as FocusedTabDataMojo,                           //
             GetPinCandidatesOptions as GetPinCandidatesOptionsMojo,         //
             HostCapability as HostCapabilityMojo,                           //
             ImageBytesResult as ImageBytesResultMojo,                       //
             ImageInfo as ImageInfoMojo,                                     //
             InvocationPayload as InvocationPayloadMojo,                     //
             InvokeOptions as InvokeOptionsMojo,                             //
             OpenPinnedTabPickerOptions as OpenPinnedTabPickerOptionsMojo,   //
             PanelOpeningData as PanelOpeningDataMojo,                       //
             PanelState as PanelStateMojo,                                   //
             PdfDocumentData as PdfDocumentDataMojo,                         //
             PinCandidate as PinCandidateMojo,                               //
             PinTabsOptions as PinTabsOptionsMojo,                           //
             Screenshot as ScreenshotMojo,                                   //
             ScreenshotCollectionOptions as ScreenshotCollectionOptionsMojo, //
             SendTabToSelfDevice as SendTabToSelfDeviceMojo,                 //
             SkillsPayload as SkillsPayloadMojo,                             //
             TabContextOptions as TabContextOptionsMojo,                     //
             TabContextResult as TabContextResultMojo,                       //
             TabData as TabDataMojo,                                         //
             UnpinTabsOptions as UnpinTabsOptionsMojo,                       //
             WebPageData as WebPageDataMojo,                                 //
             ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo,           //
             ZssConfig as ZssConfigMojo,                                     //
} from '../glic.mojom-webui.js';
import {                                                             //
  MicrophoneStatus as MicrophoneStatusMojo,                          //
  PinTrigger as PinTriggerMojo,                                      //
  ScreenshotCompressionQuality as ScreenshotCompressionQualityMojo,  //
  ScreenshotImageFormat as ScreenshotImageFormatMojo,                //
  UnpinTrigger as UnpinTriggerMojo,                                  //
  WebClientMode as WebClientModeMojo,                                //
} from '../glic.mojom-webui.js';
import type {                                                   //
             AdditionalContext,                                 //
             AdditionalContextPart,                             //
             AnnotatedPageData,                                 //
             CaptureRegionResult,                               //
             ConversationInfo,                                  //
             CounterAbuseVerdict,                               //
             CreateTabOptions,                                  //
             FileUploadPolicyState as FileUploadPolicyStateApi, //
             FocusedTabData,                                    //
             GetPinCandidatesOptions,                           //
             HostCapability,                                    //
             ImageBytesResult,                                  //
             ImageInfo,                                         //
             InvocationPayload,                                 //
             InvokeOptions,                                     //
             OpenPinnedTabPickerOptions,                        //
             PageMetadata,                                      //
             PanelOpeningData,                                  //
             PanelState,                                        //
             PdfDocumentData,                                   //
             PinCandidate,                                      //
             PinTabsOptions,                                    //
             PinTrigger,                                        //
             Screenshot,                                        //
             ScreenshotCollectionOptions,                       //
             SendTabToSelfDevice,                               //
             SkillsPayload,                                     //
             TabContextOptions,                                 //
             TabContextResult,                                  //
             TabData,                                           //
             UnpinTabsOptions,                                  //
             UnpinTrigger,                                      //
             WebPageData,                                       //
             ZeroStateSuggestionsV2,                            //
             ZssConfig,                                         //
} from '../glic_api/glic_api.js';
import {                           //
  DEFAULT_INNER_TEXT_BYTES_LIMIT,  //
  DEFAULT_PDF_SIZE_LIMIT,          //
  MicrophoneStatus,                //
  Platform,                        //
  WebClientMode,                   //
} from '../glic_api/glic_api.js';

import type {RgbaImage} from './image_utils.js';
import {ImageAlphaType, ImageColorType, rgbaImageToBlob} from './image_utils.js';

export const conversionSettings = {
  platform: undefined as Platform | undefined,
  omitFaviconInTabData: false as boolean,
};

// Helper function to shallow-copy an object and replace some properties.
// This will fail to compile if a property is missed.
export function replaceProperties<O, R>(
    original: O, replacements: R): Omit<O, keyof R>&R {
  return Object.assign({}, original, replacements);
}

function streamFromBuffer(buffer: Uint8Array): ReadableStream<Uint8Array> {
  return new ReadableStream<Uint8Array>({
    start(controller) {
      controller.enqueue(buffer);
      controller.close();
    },
  });
}

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

export function screenshotToClient(screenshot: ScreenshotMojo|null): Screenshot|
    undefined {
  if (!screenshot) {
    return undefined;
  }
  const screenshotArray = new Uint8Array(screenshot.data);
  const buffer = screenshotArray.buffer;
  return {
    widthPixels: screenshot.widthPixels,
    heightPixels: screenshot.heightPixels,
    data: buffer,
    mimeType: screenshot.mimeType,
    originAnnotations: {},
    encryptionScheme: enumToClient(screenshot.encryptionScheme),
  };
}

export function sendTabToSelfDeviceToClient(device: SendTabToSelfDeviceMojo):
    SendTabToSelfDevice {
  return {
    deviceGuid: device.deviceGuid,
    deviceName: device.deviceName,
    formFactor: enumToClient(device.formFactor),
    lastUpdatedTime: timeToClient(device.lastUpdatedTime),
  };
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
    pdfDocumentData: PdfDocumentDataMojo|null): PdfDocumentData|undefined {
  if (!pdfDocumentData) {
    return undefined;
  }
  const pdfData = pdfDocumentData.pdfData ?
      streamFromBuffer(new Uint8Array(pdfDocumentData.pdfData)) :
      undefined;
  return {
    origin: originToClient(pdfDocumentData.origin),
    pdfSizeLimitExceeded: pdfDocumentData.pdfSizeLimitExceeded,
    pdfData,
  };
}

export function protoWrapperToClient(protoWrapper: ProtoWrapper|null|undefined):
    ReadableStream<Uint8Array>|undefined {
  if (!protoWrapper) {
    return undefined;
  }
  const buffer = getArrayBufferFromBigBuffer(protoWrapper.smuggled);
  return buffer && streamFromBuffer(new Uint8Array(buffer));
}

export function annotatedPageDataToClient(
    annotatedPageData: AnnotatedPageDataMojo|null): AnnotatedPageData|
    undefined {
  if (!annotatedPageData) {
    return undefined;
  }
  const annotatedPageContent =
      protoWrapperToClient(annotatedPageData.annotatedPageContent);
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

export function originToClientOpaqueAsNull(origin: Origin): string;
export function originToClientOpaqueAsNull(origin: Origin|null): string|
    undefined;
export function originToClientOpaqueAsNull(origin: Origin|null): string|
    undefined {
  if (origin?.nonceIfOpaque) {
    return 'null';
  }
  return originToClient(origin);
}

export function tabDataToClient(tabData: TabDataMojo): TabData;
export function tabDataToClient(tabData: TabDataMojo|null): TabData|undefined;
export function tabDataToClient(tabData: TabDataMojo|null): TabData|undefined {
  if (!tabData) {
    return undefined;
  }

  let favicon: RgbaImage|undefined = undefined;
  if (tabData.favicon && !conversionSettings.omitFaviconInTabData) {
    favicon = bitmapN32ToRGBAImage(tabData.favicon);
  }
  let faviconResult: Promise<Blob>|undefined;
  async function getFavicon() {
    if (favicon && !faviconResult) {
      faviconResult = Promise.resolve(rgbaImageToBlob(favicon));
      favicon = undefined;
    }
    return faviconResult;
  }

  return {
    tabId: idToClient(tabData.tabId),
    windowId: idToClient(tabData.windowId),
    url: urlToClient(tabData.url),
    title: optionalToClient(tabData.title),
    favicon: favicon && getFavicon,
    faviconUrl: urlToClient(tabData.faviconUrl),
    documentMimeType: tabData.documentMimeType,
    isObservable: optionalToClient(tabData.isObservable),
    isMediaActive: optionalToClient(tabData.isMediaActive),
    isTabContentCaptured: optionalToClient(tabData.isTabContentCaptured),
    isActiveInWindow: optionalToClient(tabData.isActiveInWindow),
    isWindowActive: optionalToClient(tabData.isWindowActive),
    lightweightPageFeatures:
        tabData.lightweightPageFeatures?.map(feature => enumToClient(feature)),
  };
}

export function pinCandidateToClient(candidate: PinCandidateMojo):
    PinCandidate {
  return {
    tabData: tabDataToClient(candidate.tabData),
  };
}

// Microseconds between Windows epoch (1601-01-01 00:00:00 UTC) used by
// base::Time / mojo_base.mojom.Time and Unix epoch (1970-01-01 00:00:00 UTC)
// used by JS Date.
// See also:
// - chrome/browser/resources/new_tab_page/modules/calendar/common.ts
// - chrome/browser/resources/context_hub/memory_banks/memory_banks.ts
// - ui/webui/resources/cr_components/history_clusters/clusters.ts
export const WINDOWS_TO_UNIX_EPOCH_OFFSET_US: bigint = 11644473600000000n;

export function timeToClient(time: Time|null|undefined): Date|undefined {
  if (!time || time.internalValue === 0n) {
    return undefined;
  }
  const unixEpochMs =
      Number((time.internalValue - WINDOWS_TO_UNIX_EPOCH_OFFSET_US) / 1000n);
  return new Date(unixEpochMs);
}

export function focusedTabDataToClient(focusedTabData: FocusedTabDataMojo):
    FocusedTabData {
  if (focusedTabData.focusedTab) {
    return {
      hasFocus: {tabData: tabDataToClient(focusedTabData.focusedTab)},
    };
  }
  if (focusedTabData.noFocusedTabData) {
    return {
      hasNoFocus: {
        tabFocusCandidateData:
            tabDataToClient(focusedTabData.noFocusedTabData.activeTabData),
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
  if (bitmap.imageInfo.width === 0 || bitmap.imageInfo.height === 0) {
    return undefined;
  }
  const bytes = getArrayBufferFromBigBuffer(bitmap.pixelData);
  if (!bytes || bytes.byteLength === 0) {
    return undefined;
  }
  // Color type on Android is different from other platforms. Because color type
  // isn't transmitted over mojo, we need use platform to detect it here.
  const colorType = conversionSettings.platform === Platform.ANDROID ?
      ImageColorType.RGBA :
      ImageColorType.BGRA;

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
    invocationSource: enumToClient(panelOpeningData.invocationSource),
    conversationId: conversationInfo?.conversationId || undefined,
    promptSuggestion: optionalToClient(panelOpeningData.promptSuggestion),
    autoSend: panelOpeningData.autoSend && !!panelOpeningData.promptSuggestion,
    recentlyActiveConversations: panelOpeningData.recentlyActiveConversations ?
        panelOpeningData.recentlyActiveConversations.map(
            conversationInfoToClient) :
        undefined,
    conversationInfo,
    freOverride: enumToClient(panelOpeningData.freOverride),
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
    turnId: optionalToClient(conversationInfo.turnId),
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
    turnId: optionalFromClient(conversationInfo.turnId),
  };
}

export function counterAbuseVerdictFromClient(verdict: CounterAbuseVerdict):
    CounterAbuseVerdictMojo {
  return {
    sbVerdictResult: {
      url: verdict.sbVerdictResult.url,
      threatType: enumFromClient(verdict.sbVerdictResult.threatType),
      showInterstitial: verdict.sbVerdictResult.showInterstitial,
    },
  };
}

export function panelStateToClient(panelState: PanelStateMojo): PanelState {
  return {
    kind: enumToClient(panelState.kind),
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

export function tabContextToClient(tabContext: TabContextResultMojo):
    TabContextResult {
  const tabData = tabDataToClient(tabContext.tabData);
  const webPageData = webPageDataToClient(tabContext.webPageData);
  const viewportScreenshot = screenshotToClient(tabContext.viewportScreenshot);
  const screenshotInfo = protoWrapperToClient(tabContext.screenshotInfo);
  const pdfDocumentData = pdfDocumentDataToClient(tabContext.pdfDocumentData);
  const annotatedPageData =
      annotatedPageDataToClient(tabContext.annotatedPageData);

  return {
    tabData,
    webPageData,
    viewportScreenshot,
    screenshotInfo,
    pdfDocumentData,
    annotatedPageData,
  };
}

export function tabContextOptionsFromClient(options: TabContextOptions):
    TabContextOptionsMojo {
  return {
    innerText: options.innerText ?? false,
    innerTextBytesLimit:
        options.innerTextBytesLimit ?? DEFAULT_INNER_TEXT_BYTES_LIMIT,
    viewportScreenshot: options.viewportScreenshot === true ||
        options.screenshotCollectionOptions !== undefined,
    pdfData: options.pdfData ?? false,
    annotatedPageContent: options.annotatedPageContent ?? false,
    maxMetaTags: options.maxMetaTags ?? 0,
    pdfSizeLimit: options.pdfSizeLimit === undefined ?
        DEFAULT_PDF_SIZE_LIMIT :
        Math.min(Number.MAX_SAFE_INTEGER, options.pdfSizeLimit),
    annotatedPageContentMode: options.annotatedPageContentMode === undefined ?
        0 :
        options.annotatedPageContentMode,
    screenshotCollectionOptions: screenshotCollectionOptionsFromClient(
        options.screenshotCollectionOptions),
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

export function openPinnedTabPickerOptionsToMojo(
    options: OpenPinnedTabPickerOptions|
    undefined): OpenPinnedTabPickerOptionsMojo|null {
  if (!options) {
    return null;
  }
  return {};
}

export function createTabOptionsFromClient(options: CreateTabOptions = {}):
    CreateTabOptionsMojo {
  return {
    openInBackground: options.openInBackground ?? false,
    windowId: idFromClient(options.windowId),
  };
}

export function byteArrayFromClient(buffer: ArrayBuffer): number[] {
  const byteArray = new Uint8Array(buffer);
  return Array.from(byteArray);
}

export function hostCapabilitiesToClient(capabilities: HostCapabilityMojo[]):
    HostCapability[] {
  return capabilities.map(capability => enumToClient(capability));
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
  const region = result.region.rect ? {rect: result.region.rect} :
      result.region.polyline        ? {polyline: result.region.polyline} :
                                      undefined;
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
    invocationSource: enumToClient(zeroStateSuggestions.invocationSource),
  };
}

export function zssConfigToClient(zssConfig: ZssConfigMojo): ZssConfig {
  return {
    additionalContent: zssConfig.additionalContent || undefined,
  };
}

export function skillsPayloadToClient(payload: SkillsPayloadMojo):
    SkillsPayload {
  return {
    skillId: payload.skillId || '',
    skillName: payload.skillName,
    skillIcon: payload.skillIcon,
  };
}

export function invokeOptionsToClient(options: InvokeOptionsMojo):
    InvokeOptions {
  return {
    invocationSource: enumToClient(options.invocationSource),
    prompts: options.prompts || undefined,
    context: options.context ? additionalContextToClient(options.context) :
                               undefined,
    autoSubmit: options.autoSubmit,
    featureMode: enumToClient(options.featureMode),
    actuationTarget: enumToClient(options.actuationTarget),
    disableZeroStateSuggestions: options.disableZeroStateSuggestions,
    skillId: options.skillId || undefined,
    zssConfig: options.zssConfig ? zssConfigToClient(options.zssConfig) :
                                   undefined,
    payload: invocationPayloadToClient(options.payload),
    actuationTabId: idToClient(options.actuationTabId),
  };
}

export function invocationPayloadToClient(payload: InvocationPayloadMojo):
    InvocationPayload;
export function invocationPayloadToClient(payload: InvocationPayloadMojo|null):
    InvocationPayload|undefined;
export function invocationPayloadToClient(payload: InvocationPayloadMojo|null):
    InvocationPayload|undefined {
  if (!payload) {
    return undefined;
  }
  if (payload.universalCart) {
    const buffer =
        new Uint8Array(payload.universalCart.serializedMetadata).buffer;
    return {
      universalCart: {
        serializedMetadata: buffer,
      },
    };
  }
  if (payload.skillsPayload) {
    return {
      skillsPayload: skillsPayloadToClient(payload.skillsPayload),
    };
  }
  if (payload.experimentalTriggering) {
    const buffer =
        new Uint8Array(payload.experimentalTriggering.paymentMetadata).buffer;
    return {
      experimentalTriggering: {
        paymentMetadata: buffer,
      },
    };
  }
  throw new Error('Unknown payload type');
}

export function additionalContextToClient(context: AdditionalContextMojo):
    AdditionalContext {
  const parts = context.parts ?
      context.parts.map(p => additionalContextPartToClient(p)) :
      [];
  // Filter out undefined parts if any.
  const validParts: AdditionalContextPart[] = [];
  for (const p of parts) {
    if (p) {
      validParts.push(p);
    }
  }

  return {
    source: enumToClient(context.source),
    name: context.name || undefined,
    tabId: idToClient(context.tabId),
    origin: context.origin ? originToClient(context.origin) : undefined,
    frameUrl: context.frameUrl ? urlToClient(context.frameUrl) : undefined,
    parts: validParts,
  };
}

export function additionalContextPartToClient(part: AdditionalContextPartMojo):
    AdditionalContextPart|undefined {
  const result: AdditionalContextPart = {};
  if (part.data) {
    const buffer = getArrayBufferFromBigBuffer(part.data.data)!;
    result.data = new Blob([buffer], {type: part.data.mimeType});
    result.filename = optionalToClient(part.data.filename);
  } else if (part.screenshot) {
    result.screenshot = screenshotToClient(part.screenshot);
  } else if (part.webPageData) {
    result.webPageData = webPageDataToClient(part.webPageData);
  } else if (part.annotatedPageData) {
    result.annotatedPageData =
        annotatedPageDataToClient(part.annotatedPageData);
  } else if (part.pdfDocumentData) {
    result.pdf = pdfDocumentDataToClient(part.pdfDocumentData);
  } else if (part.tabContext) {
    result.tabContext = tabContextToClient(part.tabContext);
  } else if (part.region) {
    const regionMojo = part.region;
    if (regionMojo) {
      return {
        region: {
          rect: regionMojo.rect,
          polyline: regionMojo.polyline,
        },
      };
    }
  } else if (part.pendingRegion) {
    const regionMojo = part.pendingRegion.region;
    if (regionMojo) {
      return {
        pendingRegion: {
          id: part.pendingRegion.id,
          region: {
            rect: regionMojo.rect,
            polyline: regionMojo.polyline,
          },
        },
      };
    }
  } else if (part.parentConversationMetadata) {
    result.parentConversationMetadata = {
      conversationId:
          optionalToClient(part.parentConversationMetadata.conversationId),
      conversationTitle:
          optionalToClient(part.parentConversationMetadata.conversationTitle),
    };
  }
  return result;
}

export function screenshotCollectionOptionsFromClient(
    options: ScreenshotCollectionOptions|
    undefined): ScreenshotCollectionOptionsMojo {
  return {
    maxWidth: options?.maxWidth ?? 0,
    maxHeight: options?.maxHeight ?? 0,
    screenshotImageFormat: options?.screenshotImageFormat ?
        enumFromClient(options.screenshotImageFormat) :
        ScreenshotImageFormatMojo.kJpeg,
    screenshotCompressionQuality: options?.screenshotCompressionQuality ?
        enumFromClient(options.screenshotCompressionQuality) :
        ScreenshotCompressionQualityMojo.kMedium,
  };
}

export function imageInfoToClient(info: ImageInfoMojo): ImageInfo {
  return {
    caption: optionalToClient(info.caption),
    sourceOrigin: originToClientOpaqueAsNull(info.sourceOrigin),
    url: urlToClient(info.url),
    mimeType: optionalToClient(info.mimeType),
  };
}

export function imageBytesResultToClient(result: ImageBytesResultMojo):
    ImageBytesResult|null {
  const buffer = getArrayBufferFromBigBuffer(result.bytes);
  if (!buffer) {
    return null;
  }
  return {
    bytes: buffer,
    imageInfo: imageInfoToClient(result.imageInfo),
  };
}

export function fileUploadPolicyStateToClient(state: FileUploadPolicyStateMojo):
    FileUploadPolicyStateApi {
  return enumToClient(state);
}
