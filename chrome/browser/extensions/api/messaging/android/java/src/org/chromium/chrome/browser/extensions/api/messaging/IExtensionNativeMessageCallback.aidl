// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.os.Bundle;
import org.chromium.chrome.browser.extensions.api.messaging.MessagePayload;

// Represents the pipeline from the external Android app to the browser for one
// chrome.runtime.Port object or one chrome.runtime.sendNativeMessage call.
oneway interface IExtensionNativeMessageCallback {
  // Called when the external Android app sends the extension a message
  // through this port. `payload` contains the message data. `extras` may
  // contain message metadata.
  void onMessage(in MessagePayload payload, in Bundle extras);

  // Called when the external Android app disconnects this port.
  void onDisconnect();
}
