// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotatedPageData, ChromeVersion, DraggableArea, ErrorReasonTypes, ErrorWithReason, FocusedTabCandidate, FocusedTabData, InvalidCandidateError, NoCandidateTabError, OpenPanelInfo, PanelState, PdfDocumentData, Screenshot, ScrollToParams, TabContextOptions, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Request type entries should have this structure
// The name of the function, should be unique.
'name': {
  // The type of payload sent.
  request: {},
  // The type of response payload. May be 'void' for requests that do not
  // send responses.
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
    request: {},
    response: {
      microphonePermissionEnabled: boolean,
      locationPermissionEnabled: boolean,
      tabContextPermissionEnabled: boolean,
      panelState: PanelState,
      focusedTabData: FocusedTabDataPrivate,
      chromeVersion: ChromeVersion,
      canAttach: boolean,
      scrollToEnabled: boolean,
    },
  };
  // This message is sent after the client returns successfully from
  // initialize(). It is not part of the GlicBrowserHost public API.
  glicBrowserWebClientInitialized: {
    request: {success: boolean},
    response: void,
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
    request: {},
    response: void,
  };
  glicBrowserClosePanel: {
    request: {},
    response: void,
  };
  glicBrowserShowProfilePicker: {
    request: {},
    response: void,
  };
  glicBrowserGetContextFromFocusedTab: {
    request: {
      options: TabContextOptions,
    },
    response: {
      tabContextResult: TabContextResultPrivate,
    },
  };
  glicBrowserCaptureScreenshot: {
    request: {},
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
    response: void,
  };
  glicBrowserSetWindowDraggableAreas: {
    request: {
      areas: DraggableArea[],
    },
    response: void,
  };
  glicBrowserSetMicrophonePermissionState: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicBrowserSetLocationPermissionState: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicBrowserSetTabContextPermissionState: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicBrowserSetContextAccessIndicator: {
    request: {
      show: boolean,
    },
    response: void,
  };
  glicBrowserGetUserProfileInfo: {
    request: {},
    response: {
      profileInfo?: UserProfileInfoPrivate,
    },
  };
  glicBrowserRefreshSignInCookies: {
    request: {},
    response: {
      success: boolean,
    },
  };
  glicBrowserAttachPanel: {
    request: {},
    response: void,
  };
  glicBrowserDetachPanel: {
    request: {},
    response: void,
  };
  glicBrowserSetAudioDucking: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicBrowserOnUserInputSubmitted: {
    request: {
      mode: number,
    },
    response: void,
  };
  glicBrowserOnResponseStarted: {
    request: {},
    response: void,
  };
  glicBrowserOnResponseStopped: {
    request: {},
    response: void,
  };
  glicBrowserOnSessionTerminated: {
    request: {},
    response: void,
  };
  glicBrowserOnResponseRated: {
    request: {
      positive: boolean,
    },
    response: void,
  };
  glicBrowserScrollTo: {
    request: {params: ScrollToParams},
    response: void,
  };
}

// Types of requests to the GlicWebClient.
export declare interface WebClientRequestTypes {
  glicWebClientNotifyPanelWillOpen: {
    request: {
      panelState: PanelState,
    },
    response: {
      openPanelInfo?: OpenPanelInfo,
    },
  };
  glicWebClientNotifyPanelWasClosed: {
    request: {},
    response: void,
  };
  glicWebClientNotifyPanelOpened: {
    request: {
      attachedToWindowId: string|undefined,
    },
    response: void,
  };
  glicWebClientNotifyPanelClosed: {
    request: {},
    response: void,
  };
  glicWebClientPanelStateChanged: {
    request: {
      panelState: PanelState,
    },
    response: void,
  };
  glicWebClientCanAttachStateChanged: {
    request: {
      canAttach: boolean,
    },
    response: void,
  };
  glicWebClientNotifyMicrophonePermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicWebClientNotifyLocationPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicWebClientNotifyTabContextPermissionStateChanged: {
    request: {
      enabled: boolean,
    },
    response: void,
  };
  glicWebClientNotifyFocusedTabChanged: {
    request: {
      focusedTabDataPrivate: FocusedTabDataPrivate,
    },
    response: void,
  };
}

export declare interface WebClientInitialState {
  panelState?: PanelState;
}

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
export declare interface FocusedTabDataPrivate extends Omit<
    FocusedTabData, 'focusedTab'|'focusedTabCandidate'|'noCandidateTabError'> {
  focusedTab?: TabDataPrivate;
  focusedTabCandidate?: FocusedTabCandidatePrivate;
  noCandidateTabError?: NoCandidateTabError;
}

// FocusedTabDataCandidate data for postMessage transport.
export declare interface FocusedTabCandidatePrivate extends Omit<
    FocusedTabCandidate, 'focusedTabCandidateData'|'invalidCandidateError'> {
  focusedTabCandidateData?: TabDataPrivate;
  invalidCandidateError?: InvalidCandidateError;
}

// TabContextResult data for postMessage transport.
export declare interface TabContextResultPrivate extends
    Omit<TabContextResult, 'tabData'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
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
