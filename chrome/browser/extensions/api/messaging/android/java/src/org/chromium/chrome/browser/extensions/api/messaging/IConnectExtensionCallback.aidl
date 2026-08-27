// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageService;

// Callback interface implemented by the browser and passed into an
// IBrowserNativeMessageService.connectExtension call. Invoked by the external
// app to accept or reject the extension connection request.
oneway interface IConnectExtensionCallback {
  void onSuccess(IExtensionNativeMessageService service);

  void onError(String errorMessage);
}
