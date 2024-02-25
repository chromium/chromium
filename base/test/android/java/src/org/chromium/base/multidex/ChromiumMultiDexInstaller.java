// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.multidex;

import android.content.Context;
import android.os.Build;

import androidx.annotation.VisibleForTesting;
import androidx.multidex.MultiDex;

import org.chromium.base.Log;

/** Performs multidex installation for non-isolated processes. */
public class ChromiumMultiDexInstaller {
    private static final String TAG = "base_multidex";

    /**
     *  Installs secondary dexes if possible/necessary.
     *
     *  @param context The application context.
     */
    @VisibleForTesting
    public static void install(Context context) {
        // No-op on platforms that support multidex natively.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return;
        }
        MultiDex.install(context);
        Log.i(TAG, "Completed multidex installation.");
    }
}
