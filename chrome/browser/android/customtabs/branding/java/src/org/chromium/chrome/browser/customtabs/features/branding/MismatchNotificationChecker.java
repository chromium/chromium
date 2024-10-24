// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Class that drives the account mismatch notification flow. Works in conjunction with {@link
 * BrandingController} for global branding rate-limiting policy.
 */
public class MismatchNotificationChecker {
    private @NonNull final Profile mProfile;
    private @NonNull final Delegate mDelegate;

    /** Used to suppress IPH UIs while the mismatch notification UI is on the screen. */
    private Tracker.DisplayLockHandle mFeatureEngagementLock;

    /**
     * Whether the other prompt UIs should be suppressed. This var is set to {@code true} while the
     * mismatch notification UI is on the screen.
     */
    private boolean mShouldSuppressPromptUis;

    /** Interface bridging the checker with the account mismatch rate-limiting logic. */
    public interface Delegate {
        /**
         * Show the account mismatch UI if conditions are right.
         *
         * @param lastShownTime The last time the notification was shown to user.
         * @param onClose Callback to be invoked when the notification is closed.
         * @return Whether the UI is shown or not.
         */
        boolean maybeShow(long lastShownTime, Callback<Integer> onClose);
    }

    /**
     * Constructor.
     *
     * @param profile The current profile object.
     * @param delegate Delegate providing the actual decision/UI logic.
     */
    public MismatchNotificationChecker(@NonNull Profile profile, @NonNull Delegate delegate) {
        mProfile = profile;
        mDelegate = delegate;
    }

    /** Show account mismatch notification UI if all the conditions are met. */
    public boolean maybeShow(long lastShowTime) {
        boolean show = mDelegate.maybeShow(lastShowTime, this::onCloseNotification);
        if (show) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
            mFeatureEngagementLock = tracker.acquireDisplayLock();
            mShouldSuppressPromptUis = true;
        }
        return show;
    }

    private void onCloseNotification(int closeType) {
        mShouldSuppressPromptUis = false;
        if (mFeatureEngagementLock != null) mFeatureEngagementLock.release();
    }

    /** Whether prompt Ui components should be temporaily suppressed. */
    public boolean shouldSuppressPromptUis() {
        return mShouldSuppressPromptUis;
    }
}
