// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessagePort;

// Callback interface implemented by the browser and passed into an
// IExtensionNativeMessageService.connectPort call. Invoked by the external app
// to accept or reject the port connection request.
oneway interface IConnectPortCallback {
  void onSuccess(IExtensionNativeMessagePort port);

  void onError(String errorMessage);
}
