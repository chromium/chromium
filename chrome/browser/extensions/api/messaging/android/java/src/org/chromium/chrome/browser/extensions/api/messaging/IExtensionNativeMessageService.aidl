// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageCallback;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessagePort;

interface IExtensionNativeMessageService {
  // TODO(crbug.com/515159909): Add more methods here as more native messaging
  // functionality is implemented.

  // Connects a message port to the external Android app. Any messages that the
  // external Android app sends back to the browser later will be sent through
  // the callback. Used for connectNative and sendNativeMessage.
  IExtensionNativeMessagePort connectPort(IExtensionNativeMessageCallback cb);
}
