// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type WebClientInitialState} from '../glic.mojom-webui.js';
import type {ActInFocusedTabParams, ActInFocusedTabResult, AnnotatedPageData, ChromeVersion, DraggableArea, ErrorReasonTypes, ErrorWithReason, FocusedTabDataHasFocus, FocusedTabDataHasNoFocus, OpenPanelInfo, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, PdfDocumentData, Screenshot, ScrollToParams, TabContextOptions, TabContextResult, TabData, UserProfileInfo, ZeroStateSuggestions} from '../glic_api/glic_api.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Request type entries should have this structure
// The name of the function, should be unique.
name: {
  // The type of payload sent. Defaults to 'undefined', which means the request
  // has no request payload.
  request: {},
  // The type of response payload. Defaults to 'void', which means the request
  // sends no response payload.
  response: {},
}

Most requests closely match signatures of API methods. Where possible, name
messages by concatenating the interface name with the method name. This helps
readability, and ensures that each name is unique.
*/

// Types of requests to the host (Chrome).
export declare interface HostRequestTypes {
  // This message is sent just before calling initialize() on the web client.
  // It is not part of the GlicBrowserHost public API.
  glicBrowserWebClientCreated: {
    response: {
      initialState: WebClientInitialStatePrivate,
    },
  };
  // This message is sent after the client returns from initialize(). It is not
  // part of the GlicBrowserHost public API.
  glicBrowserWebClientInitialized: {
    request: {
      success: boolean,
      // Exception present if initialize() returns a rejected promise (success
      // is false).
      exception?: TransferableException,
    },
  };

  // The messages that fulfil the GlicBrowserHost public API follow below.

  glicBrowserCreateTab: {
    request: {
      url: string,
      options: {openInBackground?: boolean, windowId?: string},
    },
    response: {
      // Undefined on failure.
      tabData?: TabDataPrivate,
    },
  };
  glicBrowserOpenGlicSettingsPage: {
    request: {options?: OpenSettingsOptions},
  };
  glicBrowserClosePanel: {};
  glicBrowserClosePanelAndShutdown: {};
  glicBrowserShowProfilePicker: {};
  glicBrowserGetContextFromFocusedTab: {
    request: {
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
  };
  glicBrowserActInFocusedTab: {
    request: {
      actInFocusedTabParams: ActInFocusedTabParams,
    },
    response: {
      actInFocusedTabResult: ActInFocusedTabResultPrivate,
    },
  };
  glicBrowserStopActorTask: {
    request: {
      taskId: number,
    },
  };
  glicBrowserPauseActorTask: {
    request: {
      taskId: number,
    },
  };
  glicBrowserResumeActorTask: {
    request: {
      taskId: number,
      tabContextOptions: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
  };
  glicBrowserCaptureScreenshot: {
    response: {
      screenshot: Screenshot,
    },
  };
  glicBrowserResizeWindow: {
    request: {
      size: {
        width: number,
        height: number,
      },
      options?: {
        durationMs?: number,
      },
    },
  };
  glicBrowserEnableDragResize: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserSetWindowDraggableAreas: {
    request: {
      areas: DraggableArea[],
    },
  };
  glicBrowserSetMinimumWidgetSize: {
    request: {
      size: {
        width: number,
        height: number,
      },
    },
  };
  glicBrowserSetMicrophonePermissionState: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserSetLocationPermissionState: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserSetTabContextPermissionState: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserSetClosedCaptioningSetting: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserSetContextAccessIndicator: {
    request: {
      show: boolean,
    },
  };
  glicBrowserGetUserProfileInfo: {
    response: {
      profileInfo?: UserProfileInfoPrivate,
    },
  };
  glicBrowserRefreshSignInCookies: {
    response: {
      success: boolean,
    },
  };
  glicBrowserAttachPanel: {};
  glicBrowserDetachPanel: {};
  glicBrowserSetAudioDucking: {
    request: {
      enabled: boolean,
    },
  };
  glicBrowserOnUserInputSubmitted: {
    request: {
      mode: number,
    },
  };
  glicBrowserOnResponseStarted: {};
  glicBrowserOnResponseStopped: {};
  glicBrowserOnSessionTerminated: {};
  glicBrowserOnResponseRated: {
    request: {
      positive: boolean,
    },
  };
  glicBrowserScrollTo: {
    request: {params: ScrollToParams},
  };
  glicBrowserDropScrollToHighlight: {};
  glicBrowserSetSyntheticExperimentState: {
    request: {
      trialName: string,
      groupName: string,
    },
  };
  glicBrowserOpenOsPermissionSettingsMenu: {request: {permission: string}};
  glicBrowserGetOsMicrophonePermissionStatus: {
    response: {
      enabled: boolean,
    },
  };
  glicBrowserGetZeroStateSuggestionsForFocusedTab: {
    request: {
      isFirstRun?: boolean,
    },
    response: {
      suggestions?: ZeroStateSuggestions,
    },
  };
}

// Types of requests to the GlicWebClient.
export declare interface WebClientRequestTypes {
  glicWebClientNotifyPanelWillOpen: {
    request: {
      panelOpeningData: PanelOpeningData,
    },
    response: {
      openPanelInfo?: OpenPanelInfo,
    },
  };
  glicWebClientNotifyPanelWasClosed: {
  };
  glicWebClientPanelStateChanged: {
    request: {
      panelState: PanelState,
    },
  };
  glicWebClientCanAttachStateChanged: {
    request: {
      canAttach: boolean,
    },
  };
  glicWebClientNotifyMicrophonePermissionStateChanged: {
    request: {
      enabled: boolean,
    },
  };
  glicWebClientNotifyLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
  };
  glicWebClientNotifyTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
  };
  glicWebClientNotifyOsLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
  };
  glicWebClientNotifyClosedCaptioningSettingChanged: {
    request: {
      enabled: boolean,
    },
  };
  glicWebClientNotifyFocusedTabChanged: {
    request: {
      focusedTabDataPrivate: FocusedTabDataPrivate,
    },
  };
  glicWebClientNotifyPanelActiveChanged: {
    request: {
      panelActive: boolean,
    },
  };
  glicWebClientCheckResponsive: {};
  glicWebClientNotifyManualResizeChanged: {
    request: {
      resizing: boolean,
    },
  };
  glicWebClientBrowserIsOpenChanged: {
    request: {
      browserIsOpen: boolean,
    },
  };
  glicWebClientNotifyOsHotkeyStateChanged: {
    request: {
      hotkey: string,
    },
  };
}


type RemoveStringPrefix<S extends string, Prefix extends string> =
    S extends `${Prefix}${infer Rest}` ? Rest : 'prefixNotFound!';

type HostRequestEnumNamesType = {
  [K in keyof HostRequestTypes as RemoveStringPrefix<K, 'glicBrowser'>]: 0;
};

() => {
  // LINT.IfChange(ApiRequestType)
  // The sole purpose of this is to prompt you to update histograms.xml!
  const apiRequestTypes: HostRequestEnumNamesType = {
    WebClientCreated: 0,
    WebClientInitialized: 0,
    CreateTab: 0,
    OpenGlicSettingsPage: 0,
    ClosePanel: 0,
    ClosePanelAndShutdown: 0,
    ShowProfilePicker: 0,
    GetContextFromFocusedTab: 0,
    ActInFocusedTab: 0,
    StopActorTask: 0,
    PauseActorTask: 0,
    ResumeActorTask: 0,
    CaptureScreenshot: 0,
    ResizeWindow: 0,
    EnableDragResize: 0,
    SetWindowDraggableAreas: 0,
    SetMinimumWidgetSize: 0,
    SetMicrophonePermissionState: 0,
    SetLocationPermissionState: 0,
    SetTabContextPermissionState: 0,
    SetContextAccessIndicator: 0,
    GetUserProfileInfo: 0,
    RefreshSignInCookies: 0,
    AttachPanel: 0,
    DetachPanel: 0,
    SetAudioDucking: 0,
    OnUserInputSubmitted: 0,
    OnResponseStarted: 0,
    OnResponseStopped: 0,
    OnSessionTerminated: 0,
    OnResponseRated: 0,
    ScrollTo: 0,
    SetSyntheticExperimentState: 0,
    OpenOsPermissionSettingsMenu: 0,
    GetOsMicrophonePermissionStatus: 0,
    GetZeroStateSuggestionsForFocusedTab: 0,
    SetClosedCaptioningSetting: 0,
    DropScrollToHighlight: 0,
  };
  return apiRequestTypes;
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/histograms.xml:ApiRequestType)
};

export function requestTypeToHistogramSuffix(type: string): string|undefined {
  if (!type.startsWith('glicBrowser')) {
    return undefined;
  }
  return type.substring(11);
}

export type AllRequestTypes = HostRequestTypes&WebClientRequestTypes;
// All request types which do not provide a return.
export type AllRequestTypesWithoutReturn = {
  [K in keyof AllRequestTypes as
       RequestResponseType<K> extends void ? K : never]: AllRequestTypes[K]
};

export type RequestRequestType<T extends keyof AllRequestTypes> =
    'request' extends keyof AllRequestTypes[T] ? AllRequestTypes[T]['request'] :
                                                 undefined;
export type RequestResponseType<T extends keyof AllRequestTypes> =
    'response' extends keyof AllRequestTypes[T] ?
    AllRequestTypes[T]['response'] :
    void;

type AllValues<T> = T[keyof T];
type ArrayElement<ArrayType extends unknown[]> =
    ArrayType extends Array<infer ElementType>? ElementType : never;

// Do some high level checks that we don't accidentally add a non-cloneable or
// transferable type to our messages. These are not perfect.

// This can be extended for other transferable types when we need them. Using
// 'extends ...' for all possible Transferable types is too permissive.
type TransferableTypes = ArrayBuffer;
type StructuredClonableBasicType = string|boolean|number|void|undefined|null;
type CheckStructuredClonable<T> =
    T extends StructuredClonableBasicType ? never : T extends any[] ?
    CheckStructuredClonable<ArrayElement<T>>:
    T extends Function ?
    ['Function not structured cloneable', T] :
    T extends Promise<any>? ['Promise not structured cloneable', T] :
                            CheckStructuredClonableObject<T>;
type CheckStructuredClonableObject<T> = T extends TransferableTypes ?
    never :
    AllValues<{[K in keyof T] -?: CheckStructuredClonable<T[K]>;}>;

/* eslint-disable-next-line @typescript-eslint/naming-convention */
function assertNever<_T extends never>() {}

assertNever<CheckStructuredClonable<HostRequestTypes>>();
assertNever<CheckStructuredClonable<WebClientRequestTypes>>();

//
// Types used in messages that are not exposed directly to the API.
//
// Some types cannot be directly transported over postMessage. The pattern we
// use here is to define a new type with a 'Private' suffix, which replaces the
// property types that cannot be structured cloned, with types that can.
//
// Note that it's a good idea to replace properties with new properties that
// have the same name, but different type. This ensures that we don't
// accidentally leave the private data on the returned object.
//

// Same as A&B, but replaces properties that are in both with those in B.
type ReplaceProperties<A, B> = {
  [K in keyof A |
   keyof B]: K extends keyof B ? B[K] : K extends keyof A ? A[K] : never;
};

export type WebClientInitialStatePrivate =
    ReplaceProperties<WebClientInitialState, {
      panelState: PanelState,
      chromeVersion: ChromeVersion,
      focusedTabData: FocusedTabDataPrivate,
      loggingEnabled: boolean,
      // Whether or not the web client should resize the content to fit the
      // window size.
      fitWindow: boolean,
      enableZeroStateSuggestions: boolean,
    }>;

// TabData format for postMessage transport.
export declare interface TabDataPrivate extends Omit<TabData, 'favicon'> {
  favicon?: RgbaImage;
}

// A bitmap, used to store data from a BitmapN32 without conversion.
export declare interface RgbaImage {
  dataRGBA: ArrayBuffer;
  width: number;
  height: number;
  alphaType: ImageAlphaType;
  colorType: ImageColorType;
}

export enum ImageAlphaType {
  // RGB values are unmodified.
  UNPREMUL = 0,
  // RGB values have been premultiplied by alpha.
  PREMUL = 1,
}

// Chromium currently only uses a single color type for BitmapN32.
export enum ImageColorType {
  BGRA = 0,
}

// FocusedTabData data for postMessage transport.
export declare interface FocusedTabDataPrivate {
  hasFocus?: Omit<FocusedTabDataHasFocus, 'tabData'>&{tabData: TabDataPrivate};
  hasNoFocus?: Omit<FocusedTabDataHasNoFocus, 'tabFocusCandidateData'>&
      {tabFocusCandidateData?: TabDataPrivate};
}

// TabContextResult data for postMessage transport.
export declare interface TabContextResultPrivate extends
    Omit<TabContextResult, 'tabData'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

export declare interface ActInFocusedTabResultPrivate extends
    Omit<ActInFocusedTabResult, 'tabContextResult'> {
  tabContextResult: TabContextResultPrivate;
}

export declare interface UserProfileInfoPrivate extends
    Omit<UserProfileInfo, 'avatarIcon'> {
  avatarIcon?: RgbaImage;
}

export declare interface PdfDocumentDataPrivate extends
    Omit<PdfDocumentData, 'pdfData'> {
  pdfData?: ArrayBuffer;
}

export declare interface AnnotatedPageDataPrivate extends
    Omit<AnnotatedPageData, 'annotatedPageContent'> {
  annotatedPageContent?: ArrayBuffer;
  metadata?: PageMetadata;
}

export class ErrorWithReasonImpl<T extends keyof ErrorReasonTypes> extends Error
    implements ErrorWithReason<T> {
  constructor(
      public reasonType: T,
      public reason: ErrorReasonTypes[T],
      message?: string,
  ) {
    super(message ?? `${reasonType} Error: ${reason}`);
  }
}

/** Any ErrorWithReason<T>.reason type. */
export type AnyErrorReasonType = ErrorReasonTypes[keyof ErrorReasonTypes];
/** Any ErrorWithReason type. */
export type AnyErrorWithReasonType = ErrorWithReason<keyof ErrorReasonTypes>;
/** Sent in ResponseMessage to reconstruct the ErrorWithReason. */
export interface ErrorWithReasonDetails {
  reason: AnyErrorReasonType;
  reasonType: keyof ErrorReasonTypes;
}

// Exception information that can be passed across postMessage.
export interface TransferableException {
  // An error that occurred during processing the request.
  exception: Error;
  // This may be set to indicate that the exception is a ErrorWithReason
  // exception.
  exceptionReason?: ErrorWithReasonDetails;
}

// Constructs an exception from a TransferableException.
export function exceptionFromTransferable(e: TransferableException): Error|
    AnyErrorWithReasonType {
  // Error types are serializable, but they do not serialize all members.
  // If exceptionReason is provided, we use it to reconstruct a
  // ErrorWithReason by just setting additional fields after
  // serialization.
  if (e.exceptionReason !== undefined) {
    const withReason = e.exception as AnyErrorWithReasonType;
    withReason.reason = e.exceptionReason.reason;
    withReason.reasonType = e.exceptionReason.reasonType;
  }

  return e.exception;
}

// Transform an Error into a TransferableException.
export function newTransferableException(e: Error): TransferableException {
  let exceptionReason = undefined;
  const maybeWithReason = e as Partial<AnyErrorWithReasonType>;
  if (maybeWithReason.reasonType !== undefined &&
      maybeWithReason.reason !== undefined) {
    exceptionReason = {
      reason: maybeWithReason.reason,
      reasonType: maybeWithReason.reasonType,
    };
  }
  return {exception: e, exceptionReason};
}
