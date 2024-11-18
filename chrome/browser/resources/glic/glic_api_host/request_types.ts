// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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
export interface HostRequestTypes {
  'glicBrowserGetChromeVersion': {
    request: {},
    response: {
      major: number,
      minor: number,
      build: number,
      patch: number,
    },
  };
}

// Types of requests to the GlicWebClient.
export interface WebClientRequestTypes {
  'glicWebClientNotifyPanelOpened': {
    request: {
      dockedToWindowId: string|undefined,
    },
    response: void,
  };
}
