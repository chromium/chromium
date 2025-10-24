// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.view.Window;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.UiUtils;

/** A set of helper functions related to gesture navigation. */
@JNINamespace("gesturenav")
@NullMarked
public class GestureNavigationUtils {

    /**
     * Whether the default nav transition should be allowed for the current tab.
     *
     * @param tab The current tab.
     * @param forward True if navigating forward; false if navigating back.
     * @return True if the transition should be enabled for this tab when navigating..
     */
    public static boolean allowTransition(@Nullable Tab tab, boolean forward) {
        if (tab == null) return false;
        if (!shouldAnimateBackForwardTransitions()) return false;
        // If in gesture mode, only U and above support transition.
        Window window = tab.getWindowAndroidChecked().getWindow();
        if (window == null) return false;
        if (VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE
                && UiUtils.isGestureNavigationMode(window)) {
            return false;
        }
        return true;
    }

    /**
     * @return Whether the back forward transitions are enabled.
     */
    public static boolean shouldAnimateBackForwardTransitions() {
        return GestureNavigationUtilsJni.get().shouldAnimateBackForwardTransitions();
    }

    public static void setMinRequiredPhysicalRamMbForTesting(int mb) {
        Runnable resetAfterTesting =
                GestureNavigationUtilsJni.get()
                        .setMinRequiredPhysicalRamMbForTesting(mb); // IN-TEST
        ResettersForTesting.register(resetAfterTesting);
    }

    @NativeMethods
    public interface Natives {
        boolean shouldAnimateBackForwardTransitions();

        Runnable setMinRequiredPhysicalRamMbForTesting(int mb); // IN-TEST
    }
}
