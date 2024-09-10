// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import org.chromium.base.ServiceLoaderUtil;

/** A launcher for Instant Apps. */
public class InstantAppsHandler {
    private static final Object INSTANCE_LOCK = new Object();
    private static InstantAppsHandler sInstance;

    /** @return The singleton instance of {@link InstantAppsHandler}. */
    public static InstantAppsHandler getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                InstantAppsHandler instance =
                        ServiceLoaderUtil.maybeCreate(InstantAppsHandler.class);
                if (instance == null) {
                    instance = new InstantAppsHandler();
                }
                sInstance = instance;
            }
        }
        return sInstance;
    }

    /**
     * Returns whether or not the instant app is available.
     *
     * @param url The URL where the instant app is located.
     * @param checkHoldback Check if the app would be available if the user weren't in the holdback
     *        group.
     * @param includeUserPrefersBrowser Function should return true if there's an instant app intent
     *        even if the user has opted out of instant apps.
     * @return Whether or not the instant app specified by the entry in the page's manifest is
     *         either available, or would be available if the user wasn't in the holdback group.
     */
    public boolean isInstantAppAvailable(
            String url, boolean checkHoldback, boolean includeUserPrefersBrowser) {
        return false;
    }
}
