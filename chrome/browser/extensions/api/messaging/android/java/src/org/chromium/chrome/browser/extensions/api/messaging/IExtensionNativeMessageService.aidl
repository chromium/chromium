// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import org.chromium.chrome.browser.extensions.api.messaging.IConnectPortCallback;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageCallback;

oneway interface IExtensionNativeMessageService {
  // Called by the browser when the extension is no longer enabled (i.e. it is
  // disabled or uninstalled).
  void closeConnection();

  // Connects a message port to the external Android app. Any messages that the
  // external Android app sends back to the browser later will be sent through
  // `messageReceiver`. Used for connectNative and sendNativeMessage.
  // callback: Invoked by the external app to accept or reject the port
  // connection request.
  void connectPort(
      IExtensionNativeMessageCallback messageReceiver,
      IConnectPortCallback callback);
}
