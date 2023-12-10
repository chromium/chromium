// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.permission;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.android_webview.CleanupReference;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.lang.ref.WeakReference;

/**
 * This class implements AwGeolocationPermissions.Callback, and will be sent to
 * WebView applications through WebChromeClient.onGeolocationPermissionsShowPrompt().
 */
public class AwGeolocationCallback implements AwGeolocationPermissions.Callback {
    private static final String TAG = "Geolocation";

    private CleanupRunable mCleanupRunable;
    private CleanupReference mCleanupReference;

    private static class CleanupRunable implements Runnable {
        private WeakReference<AwContents> mAwContents;
        private boolean mAllow;
        private boolean mRetain;
        private String mOrigin;

        public CleanupRunable(AwContents awContents, String origin) {
            mAwContents = new WeakReference<AwContents>(awContents);
            mOrigin = origin;
        }

        @Override
        public void run() {
            assert ThreadUtils.runningOnUiThread();
            AwContents awContents = mAwContents.get();
            if (awContents == null) return;
            if (mRetain) {
                if (mAllow) {
                    awContents.getGeolocationPermissions().allow(mOrigin);
                } else {
                    awContents.getGeolocationPermissions().deny(mOrigin);
                }
            }
            awContents.invokeGeolocationCallback(mAllow, mOrigin);
        }

        public void setResponse(String origin, boolean allow, boolean retain) {
            mOrigin = origin;
            mAllow = allow;
            mRetain = retain;
        }
    }

    public AwGeolocationCallback(String origin, AwContents awContents) {
        mCleanupRunable = new CleanupRunable(awContents, origin);
        mCleanupReference = new CleanupReference(this, mCleanupRunable);
    }

    @Override
    public void invoke(String origin, boolean allow, boolean retain) {
        if (mCleanupRunable == null || mCleanupReference == null) {
            Log.w(
                    TAG,
                    "Response for this geolocation request has been received."
                            + " Ignoring subsequent responses");
            return;
        }
        mCleanupRunable.setResponse(origin, allow, retain);
        mCleanupReference.cleanupNow();
        mCleanupReference = null;
        mCleanupRunable = null;
    }
}
