// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.Log;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/** Preloads expensive classes that will be used by WebView.. */
public final class AwClassPreloader {
    private static final String TAG = "AwClassPreloader";

    private AwClassPreloader() {}

    private static void preloadClass(Class<?> clz) {
        String name = clz.getName();
        // Make sure the class is fully loaded.
        try {
            Class.forName(name);
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Failed to preload class: " + name, e);
        }
    }

    /** Preloads a set of classes on a background thread. */
    public static void preloadClasses() {
        if (!AwFeatureMap.isEnabled("WebViewPreloadClasses")) {
            return;
        }
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    try (ScopedSysTraceEvent e =
                            ScopedSysTraceEvent.scoped("AwClassPreloader.preloadClasses")) {
                        // This set of classes was chosen by local instrumentation of the class
                        // loader to determine which classes were slow during initialization.
                        for (Class<?> clz :
                                new Class<?>[] {
                                    org.chromium.android_webview.AwContents.HitTestData.class,
                                    org.chromium.android_webview.AwContents.class,
                                    org.chromium.android_webview.AwContentsClient.class,
                                    org.chromium.android_webview.AwDisplayModeController.class,
                                    org.chromium.android_webview.CleanupReference.class,
                                    org.chromium.content.browser.selection
                                            .SelectionPopupControllerImpl.class,
                                    org.chromium.content.browser.webcontents.WebContentsImpl.class,
                                    org.chromium.ui.base.WindowAndroid.class,
                                }) {
                            preloadClass(clz);
                        }
                    }
                });
    }
}
