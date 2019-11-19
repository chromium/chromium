// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

/**
 * Type of WebApkActivity and it is targeted on Android versions older than L, similar to
 * WebappManagedActivity for WebappActivity.
 */
public abstract class WebApkManagedActivity extends WebApkActivity {
    private final int mActivityIndex;

    public WebApkManagedActivity() {
        mActivityIndex = getActivityIndex();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();

        if (!isFinishing()) {
            markActivityUsed();
        }
    }

    @Override
    protected String getActivityId() {
        return WebappRegistry.webApkIdForPackage(String.valueOf(mActivityIndex));
    }

    /**
     * Marks that this WebApkActivity is recently used to prevent other webapps from using it.
     */
    private void markActivityUsed() {
        ActivityAssigner.instance(ActivityAssigner.ActivityAssignerNamespace.WEBAPK_NAMESPACE)
                .markActivityUsed(mActivityIndex, getWebappInfo().id());
    }

    /**
     * Pulls out the index of the WebApkActivity subclass that is being used.
     * e.g. WebApkActivity0.getActivityIndex() will return 0.
     * @return The index corresponding to this WebApkActivity0.
     */
    private int getActivityIndex() {
        // Cull out the activity index from the class name.
        String baseClassName = WebApkActivity.class.getSimpleName();
        String className = this.getClass().getSimpleName();
        assert className.matches("^" + baseClassName + "[0-9]+$");
        String indexString = className.substring(baseClassName.length());
        return Integer.parseInt(indexString);
    }
}
