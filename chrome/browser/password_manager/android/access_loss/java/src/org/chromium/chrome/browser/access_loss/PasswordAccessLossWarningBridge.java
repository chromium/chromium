// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import org.jni_zero.CalledByNative;

class PasswordAccessLossWarningBridge {

    @CalledByNative
    private static PasswordAccessLossWarningBridge create() {
        return new PasswordAccessLossWarningBridge();
    }

    @CalledByNative
    private void show() {
        // TODO: crbug.com/356627466 - Show the sheet.
    }
}
