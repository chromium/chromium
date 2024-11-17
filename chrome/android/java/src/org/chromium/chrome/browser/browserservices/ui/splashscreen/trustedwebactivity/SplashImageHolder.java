// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity;

import android.graphics.Bitmap;
import android.util.ArrayMap;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import java.util.Collections;
import java.util.Map;

/**
 * Stores the splash images received from TWA clients between the call to {@link
 * androidx.browser.customtabs.CustomTabsService#receiveFile} and a Trusted Web Activity claiming
 * the image.
 *
 * <p>This class is thread-safe.
 */
public class SplashImageHolder {
    private final Map<CustomTabsSessionToken, Bitmap> mBitmaps =
            Collections.synchronizedMap(new ArrayMap<>());

    private static SplashImageHolder sInstance;

    public static SplashImageHolder getInstance() {
        if (sInstance == null) sInstance = new SplashImageHolder();
        return sInstance;
    }

    private SplashImageHolder() {}

    /**
     * Puts the bitmap into cache. It is expected to be retrieved shortly thereafter using {@link
     * #takeImage}.
     */
    public void putImage(CustomTabsSessionToken token, Bitmap bitmap) {
        mBitmaps.put(token, bitmap);
    }

    /** Takes the bitmap out of the cache. */
    public @Nullable Bitmap takeImage(CustomTabsSessionToken token) {
        return mBitmaps.remove(token);
    }
}
