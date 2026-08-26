// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.os.SharedMemory;

// Represents a message sent from the browser on behalf of an extension to an
// external app or from an external app to the browser. The type used in the
// union depends on the message's size.
union MessagePayload {
  byte[] inlineBytes = {};
  SharedMemory sharedMemory;
}
