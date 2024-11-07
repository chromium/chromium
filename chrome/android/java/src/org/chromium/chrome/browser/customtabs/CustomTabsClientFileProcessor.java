// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.WorkerThread;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.SplashImageHolder;

/** Processes the files received via Custom Tab connection from client apps. */
public class CustomTabsClientFileProcessor {

    private static final String TAG = "CustomTabFiles";

    private boolean mTwaSplashImageHolderCreated;

    private static CustomTabsClientFileProcessor sInstance;

    public static CustomTabsClientFileProcessor getInstance() {
        if (sInstance == null) sInstance = new CustomTabsClientFileProcessor();
        return sInstance;
    }

    private CustomTabsClientFileProcessor() {}

    /**
     * Processes the file located at given URI.
     *
     * @return {@code true} if successful.
     */
    @WorkerThread
    public boolean processFile(
            CustomTabsSessionToken session, Uri uri, int purpose, Bundle extras) {
        if (uri == null) {
            Log.w(TAG, "Received a null uri");
            return false;
        }
        switch (purpose) {
            case CustomTabsService.FILE_PURPOSE_TRUSTED_WEB_ACTIVITY_SPLASH_IMAGE:
                return receiveTwaSplashImage(session, uri);
        }
        Log.w(TAG, "Unknown FilePurpose " + purpose);
        return false;
    }

    private boolean receiveTwaSplashImage(CustomTabsSessionToken sessionToken, Uri uri) {
        Bitmap bitmap =
                FileUtils.queryBitmapFromContentProvider(ContextUtils.getApplicationContext(), uri);
        if (bitmap == null) return false;

        SplashImageHolder.getInstance().putImage(sessionToken, bitmap);
        mTwaSplashImageHolderCreated = true;
        return true;
    }

    /** Cleans up files associated with the session that has been disconnected. */
    public void onSessionDisconnected(CustomTabsSessionToken session) {
        if (mTwaSplashImageHolderCreated) {
            // If the image still hasn't been claimed, delete it.
            SplashImageHolder.getInstance().takeImage(session);
        }
    }
}
