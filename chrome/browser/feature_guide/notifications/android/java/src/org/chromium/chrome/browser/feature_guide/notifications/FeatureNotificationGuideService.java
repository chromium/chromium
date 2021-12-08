// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

/**
 * Central class representing feature notification guide.
 */
public interface FeatureNotificationGuideService {
    /**
     * A delegate containing helper methods needed by the service, such as providing notification
     * texts, and handling notification interactions.
     */
    public interface Delegate {
        /**
         * Called when the notification associated with {@code featureType} is clicked.
         * @param featureType The {@link FeatureType} for the notification.
         */
        void onNotificationClick(@FeatureType int featureType);

        /**
         * Called to get the notification title text associated with {@code featureType}.
         * @param featureType The {@link FeatureType} for the notification.
         */
        String getNotificationTitle(@FeatureType int featureType);

        /**
         * Called to get the notification body text associated with {@code featureType}.
         * @param featureType The {@link FeatureType} for the notification.
         */
        String getNotificationMessage(@FeatureType int featureType);
    }

    /**
     * Called by the embedder to set the delegate.
     * @param delegate The {@link Delegate} to handle chrome layer logic.
     */
    void setDelegate(Delegate delegate);
}