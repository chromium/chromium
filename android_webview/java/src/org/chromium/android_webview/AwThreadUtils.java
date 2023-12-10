// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;

import org.chromium.base.ThreadUtils;

/** Provides WebView-specific threading utilities. */
public class AwThreadUtils {
    /**
     * Post a task to the current thread, ensuring that it runs on the underlying Android looper
     * without any native code present on the stack. This allows uncaught Java exceptions to be
     * handled correctly by Android's crash reporting mechanisms.
     */
    public static void postToCurrentLooper(Runnable r) {
        new Handler().post(r);
    }

    /**
     * Post a task to the UI thread, ensuring that it runs on the underlying Android looper without
     * any native code present on the stack. This allows uncaught Java exceptions to be handled
     * correctly by Android's crash reporting mechanisms.
     */
    public static void postToUiThreadLooper(Runnable r) {
        ThreadUtils.getUiThreadHandler().post(r);
    }
}
