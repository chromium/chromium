// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.os.Bundle;
import org.chromium.chrome.browser.extensions.api.messaging.IConnectExtensionCallback;

oneway interface IBrowserNativeMessageService {
  // Called by the browser on behalf of the extension for its first native
  // messaging call.
  // extensionId: The string ID of the extension.
  // extensionInfo: Contains additional extension metadata. This includes an
  // "isVerified" boolean flag indicating if the extension's contents are
  // verified against a source of truth.
  // callback: Invoked by the external app to accept or reject the extension
  // connection request.
  void connectExtension(
      String extensionId, in Bundle extensionInfo,
      IConnectExtensionCallback callback);
}
