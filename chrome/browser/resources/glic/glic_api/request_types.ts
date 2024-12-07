// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabContextResult, TabData} from '../glic_api/glic_api.js';

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
      tabData?: TabData,
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
      // Undefined on failure.
      tabContextResult?: TabContextResult,
    },
  };
  glicBrowserResizeWindow: {
    request: {
      width: number,
      height: number,
    },
    response: {
      actualWidth: number,
      actualHeight: number,
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
}
