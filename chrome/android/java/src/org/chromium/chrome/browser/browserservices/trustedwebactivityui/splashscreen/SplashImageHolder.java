// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen;

import android.graphics.Bitmap;
import android.util.ArrayMap;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import java.util.Collections;
import java.util.Map;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * Stores the splash images received from TWA clients between the call to
 * {@link android.support.customtabs.CustomTabsService#receiveFile} and a Trusted Web Activity
 * claiming the image.
 *
 * This class is thread-safe.
 */
@Singleton
public class SplashImageHolder {

    private final Map<CustomTabsSessionToken, Bitmap> mBitmaps =
            Collections.synchronizedMap(new ArrayMap<>());

    @Inject
    public SplashImageHolder() {}

    /**
     * Puts the bitmap into cache. It is expected to be retrieved shortly thereafter using
     * {@link #takeImage}.
     */
    public void putImage(CustomTabsSessionToken token, Bitmap bitmap) {
        mBitmaps.put(token, bitmap);
    }

    /**
     * Takes the bitmap out of the cache.
     */
    @Nullable
    public Bitmap takeImage(CustomTabsSessionToken token) {
        return mBitmaps.remove(token);
    }
}
