// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.os.Bundle;
import org.chromium.chrome.browser.extensions.api.messaging.IExtensionNativeMessageService;

interface IBrowserNativeMessageService {
  // Called by Chrome on behalf of the extension for its first native messaging
  // call.
  // ExtensionId: The string ID of the extension
  // TODO(crbug.com/515159909): Remove this @Nullable once this information is available.
  IExtensionNativeMessageService connectExtension(
      String extensionId, in @nullable Bundle extensionInfo);
}
