// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating instances of {@link AppRatingManager}. */
@NullMarked
public class AppRatingManagerFactory {
    private static @Nullable AppRatingManager sManagerForTesting;

    /**
     * Returns an instance of {@link AppRatingManager}. If the Play Services implementation is not
     * available, it returns a no-op instance.
     */
    public static AppRatingManager create() {
        if (sManagerForTesting != null) return sManagerForTesting;

        AppRatingManager impl = ServiceLoaderUtil.maybeCreate(AppRatingManager.class);
        if (impl != null) {
            return impl;
        }

        // Placeholder no-op implementation for non-branded builds.
        return new AppRatingManager() {
            @Override
            public void requestAndShowReviewFlow(Activity activity, Runnable onComplete) {
                onComplete.run();
            }
        };
    }

    /**
     * Set a manager for testing.
     *
     * @param manager The manager.
     */
    public static void setInstanceForTesting(@Nullable AppRatingManager manager) {
        sManagerForTesting = manager;
    }
}
