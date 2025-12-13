// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefs;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.prefs.PrefService;

/** Helper for retrieving the Local State {@link PrefService}. */
@JNINamespace("chrome_browser_prefs")
@NullMarked
public class LocalStatePrefs {
    private static boolean sIsNativeReady;

    /** Returns the {@link PrefService} associated with local state. */
    public static @Nullable PrefService get() {
        if (!areNativePrefsLoaded()) return null;
        return LocalStatePrefsJni.get().getPrefService();
    }

    /** Returns whether the native side is initialized. */
    public static boolean areNativePrefsLoaded() {
        return sIsNativeReady;
    }

    @CalledByNative
    private static void setNativePrefsLoaded() {
        sIsNativeReady = true;
    }

    public static void setNativePrefsLoadedForTesting(boolean state) {
        boolean oldState = sIsNativeReady;
        sIsNativeReady = state;
        ResettersForTesting.register(
                () -> {
                    sIsNativeReady = oldState;
                });
    }

    @NativeMethods
    public interface Natives {
        PrefService getPrefService();
    }
}
