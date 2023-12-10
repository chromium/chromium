// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

/** Central class representing feature notification guide. */
public abstract class FeatureNotificationGuideService {
    /**
     * Delegate to be provide chrome app layer dependencies. Owned by the {@link
     * FeatureNotificationGuideService}, so shouldn't hold any reference to the activities.
     */
    public interface Delegate {
        /**
         * Launches an activity to show IPH when a feature notification is clicked.
         * @param featureType The type of the feature being promoed in the notification.
         */
        void launchActivityToShowIph(@FeatureType int featureType);
    }

    /**
     * The delegate is designed as static so that it can be set even before the service
     * is instantiated.
     */
    private static Delegate sDelegate;

    /** Called to set the delegate. */
    public static void setDelegate(Delegate delegate) {
        assert sDelegate == null && delegate != null;
        sDelegate = delegate;
    }

    /** @return The {@link Delegate} to provide chrome app layer dependencies. */
    public static Delegate getDelegate() {
        assert sDelegate != null;
        return sDelegate;
    }
}
