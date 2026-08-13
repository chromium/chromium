// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

// Represents the pipeline from the external Android app to the browser for one
// chrome.runtime.Port object or one chrome.runtime.sendNativeMessage call.
oneway interface IExtensionNativeMessageCallback {
  // Called when the external Android app sends the extension a message
  // through this port. `message` must be a JSON-serialized string.
  void onMessage(String message);

  // Called when the external Android app disconnects this port.
  void onDisconnect();
}
