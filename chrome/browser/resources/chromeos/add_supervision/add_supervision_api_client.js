// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const METHOD_LIST = ['logOut', 'getInstalledArcApps'];

/**
 * Class that implements the client side of the AddSupervision postMessage API.
 * All of the actual functionality is in the superclass.  Family Link uses
 * its own implementation of this client library to meet their separate
 * google dev and deployment processes and standards, so this file serves
 * as a reference implementation, and can be used by end-to-end tests of the
 * API's behavior.  It is not used in the build, but it is included by
 * webview-example.html and webview-example2.html, which demonstrate how
 * to use the API from an embedded webview.
 */
class AddSupervisionAPIClient extends PostMessageAPIClient {
  constructor() {
    super(METHOD_LIST);
  }
}
