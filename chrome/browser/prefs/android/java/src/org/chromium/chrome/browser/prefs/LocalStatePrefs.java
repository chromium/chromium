// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefs;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.prefs.PrefService;

/** Helper for retrieving the Local State {@link PrefService}. */
@JNINamespace("chrome_browser_prefs")
@NullMarked
public class LocalStatePrefs {
    /** Returns the {@link PrefService} associated with local state. */
    public static PrefService get() {
        return LocalStatePrefsJni.get().getPrefService();
    }

    @NativeMethods
    public interface Natives {
        PrefService getPrefService();
    }
}
