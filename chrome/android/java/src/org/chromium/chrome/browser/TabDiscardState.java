// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

/** Provides per-tab discardability diagnostics (mirroring chrome://discards) for dumpsys. */
@JNINamespace("discards")
@NullMarked
public class TabDiscardState {
    private TabDiscardState() {}

    /**
     * Returns a formatted string with per-tab discardability diagnostics, safe to call from any
     * thread.
     */
    public static String getTabDiscardsDump() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
                        return "Browser not started.\n";
                    }
                    return TabDiscardStateJni.get().getTabDiscardsDump();
                });
    }

    @NativeMethods
    public interface Natives {
        @JniType("std::string")
        String getTabDiscardsDump();
    }
}
