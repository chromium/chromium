// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Java peer for the AwLoadUrlMetricsObserver C++ class. */
@JNINamespace("android_webview")
@NullMarked
public class AwLoadUrlMetricsObserver {
    public static void setPendingLoadUrlTimestamp(long uptimeMillis, WebContents webContents) {
        if (webContents == null) return;
        AwLoadUrlMetricsObserverJni.get().setPendingLoadUrlTimestamp(uptimeMillis, webContents);
    }

    @NativeMethods
    interface Natives {
        void setPendingLoadUrlTimestamp(long uptimeMillis, WebContents webContents);
    }
}
