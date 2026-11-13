// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefs;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.prefs.PrefService;

/** Helper for retrieving the Local State {@link PrefService}. */
@JNINamespace("chrome_browser_prefs")
@NullMarked
public class LocalStatePrefs {
    private static boolean sNativePrefsLoaded;
    private static @Nullable PrefService sPrefServiceForTesting;

    /** Returns the {@link PrefService} associated with local state. */
    public static PrefService get() {
        if (BuildConfig.IS_ROBOLECTRIC) {
            if (sPrefServiceForTesting == null) {
                sPrefServiceForTesting = PrefService.createForTesting(); // IN-TEST
                ResettersForTesting.register(() -> sPrefServiceForTesting = null); // IN-TEST
            }
            return sPrefServiceForTesting;
        }
        assert sNativePrefsLoaded : "LocalStatePrefs accessed before native initialization";
        return LocalStatePrefsJni.get().getPrefService();
    }

    @CalledByNative
    private static void setNativePrefsLoaded() {
        sNativePrefsLoaded = true;
    }

    @NativeMethods
    interface Natives {
        PrefService getPrefService();
    }
}
