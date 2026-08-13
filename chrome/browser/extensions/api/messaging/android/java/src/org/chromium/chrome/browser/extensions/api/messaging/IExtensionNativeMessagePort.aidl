// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

// Represents the pipeline from the browser to the external Android app for one
// chrome.runtime.Port object or one chrome.runtime.sendNativeMessage call.
oneway interface IExtensionNativeMessagePort {
  // Called when the browser forwards a message to the external Android app
  // through the port on behalf of the extension. `message` is a
  // JSON-serialized string.
  void postMessage(String message);

  // Called when the browser disconnects this port from the external Android
  // app. No future messages will be sent through this port.
  void disconnect();
}
