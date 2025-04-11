// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.serial;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/** Java access point for SerialBridge, allowing for querying serial port state. */
@NullMarked
public class SerialBridge {
    public static boolean isWebContentsConnectedToSerialPort(@Nullable WebContents webContents) {
        if (webContents == null) return false;
        return SerialBridgeJni.get().isWebContentsConnectedToSerialPort(webContents);
    }

    @NativeMethods
    interface Natives {
        boolean isWebContentsConnectedToSerialPort(WebContents webContents);
    }
}
