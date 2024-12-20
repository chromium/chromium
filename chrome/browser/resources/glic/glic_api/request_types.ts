// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DraggableArea, GetTabContextErrorReason, PanelState, TabContextResult, TabData, UserProfileInfo} from '../glic_api/glic_api.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

CHANGES MADE HERE MUST BE BACKWARDS COMPATIBLE.

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
  // This message is sent after the client returns successfully from
  // initialize(). It is not part of the GlicBrowserHost public API.
  glicBrowserWebClientInitialized: {
    request: {},
    response: void,
  };
  glicBrowserGetChromeVersion: {
    request: {},
    response: {
      major: number,
      minor: number,
      build: number,
      patch: number,
    },
  };
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
  glicBrowserClosePanel: {
    request: {},
    response: void,
  };
  glicBrowserGetContextFromFocusedTab: {
    request: {
      options: {
        innerText?: boolean,
        // Options for capturing screenshot, currently none supported.
        viewportScreenshot?: boolean,
      },
    },
    response: {
      // Present on success.
      tabContextResult?: TabContextResultPrivate,
      // The error reason. Should be present when `tabContextResult` is not, but
      // might still be undefined for some older chrome versions.
      error?: GetTabContextErrorReason,
    },
  };
  glicBrowserResizeWindow: {
    request: {
      width: number,
      height: number,
    },
    response: {
      // Not set on error.
      actualWidth?: number,
      actualHeight?: number,
    },
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
  glicBrowserGetUserProfileInfo: {
    request: {},
    response: {
      profileInfo?: UserProfileInfoPrivate,
    },
  };
}

// Types of requests to the GlicWebClient.
export declare interface WebClientRequestTypes {
  glicWebClientNotifyPanelOpened: {
    request: {
      dockedToWindowId: string|undefined,
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
}

export declare interface WebClientInitialState {
  panelState?: PanelState;
}

//
// Types used in messages that are not exposed directly to the API.
//

// TabData format for postMessage transport.
export declare interface TabDataPrivate extends Omit<TabData, 'favicon'> {
  rawFavicon?: RgbaImage;
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

// TabContextResult data for postMessage transport.
export declare interface TabContextResultPrivate extends
    Omit<TabContextResult, 'tabData'> {
  tabData: TabDataPrivate;
}

export declare interface UserProfileInfoPrivate extends
    Omit<UserProfileInfo, 'avatarIcon'> {
  avatarIconImage?: RgbaImage;
}
