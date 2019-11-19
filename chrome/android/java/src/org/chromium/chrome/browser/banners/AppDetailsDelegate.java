// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import androidx.annotation.VisibleForTesting;

/**
 * Fetches data about the given app.
 */
@VisibleForTesting
public abstract class AppDetailsDelegate {
    /**
     * Class to inform when the app's details have been retrieved.
     */
    public interface Observer {
        /**
         * Called when the task has finished.
         * @param data Data about the requested package.  Will be null if retrieval failed.
         */
        @VisibleForTesting
        public void onAppDetailsRetrieved(AppData data);
    }

    /**
     * Retrieves information about the given package asynchronously.  When details have been
     * retrieved, the observer is alerted.
     * @param observer    Informed when the app details have been received.
     * @param url         URL of the page requesting a banner.
     * @param packageName Name of the app's package.
     * @param referrer    Referrer specified by the page requesting a banner.
     * @param iconSize    Size of the icon to retrieve.
     */
    protected abstract void getAppDetailsAsynchronously(
            Observer observer, String url, String packageName, String referrer, int iconSize);

    /**
     * Destroy the delegate, cleaning up any open hooks.
     */
    public abstract void destroy();
}
