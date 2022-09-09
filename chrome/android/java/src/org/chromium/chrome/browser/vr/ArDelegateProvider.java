// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.Log;
import org.chromium.components.webxr.ArDelegate;

/**
 * Class used to create ArDelegate instances.
 */
public class ArDelegateProvider {
    private static final String TAG = "ArDelegateProvider";
    private static final boolean DEBUG_LOGS = false;

    /**
     * Cached instance of ArDelegate implementation. It is ok to cache since the
     * inclusion of ArDelegateImpl is controlled at build time.
     */
    private static ArDelegate sDelegate;

    /**
     * True if sDelegate already contains cached result, false otherwise.
     */
    private static boolean sDelegateInitialized;

    /**
     * Provides an instance of ArDelegate.
     */
    public static ArDelegate getDelegate() {
        if (DEBUG_LOGS) {
            Log.i(TAG,
                    "ArDelegate.getDelegate(): sDelegateInitialized=" + sDelegateInitialized
                            + ", is sDelegate null? " + (sDelegate == null));
        }

        if (sDelegateInitialized) return sDelegate;

        try {
            sDelegate = (ArDelegate) Class.forName("org.chromium.chrome.browser.vr.ArDelegateImpl")
                                .newInstance();
        } catch (ClassNotFoundException e) {
        } catch (InstantiationException e) {
        } catch (IllegalAccessException e) {
        } finally {
            sDelegateInitialized = true;
        }

        if (DEBUG_LOGS) {
            Log.i(TAG, "Is sDelegate null? " + (sDelegate == null));
        }

        return sDelegate;
    }
}
