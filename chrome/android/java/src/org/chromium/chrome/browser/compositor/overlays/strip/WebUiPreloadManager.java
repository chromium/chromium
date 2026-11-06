// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.ui.base.WindowAndroid;

/**
 * Observes {@link ProfileManager} initialization to prewarm top-chrome WebUI surfaces as soon as
 * the {@link Profile} is created during browser startup, before post-native {@code ChainedTasks}
 * run.
 */
@NullMarked
public class WebUiPreloadManager implements ProfileManager.Observer, DestroyObserver {
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final @SupportedProfileType int mSupportedProfileType;
    private final long mActivityCreationUptimeMs;

    /**
     * Preloads enabled top-chrome WebUI surfaces for the given window once {@link ProfileManager}
     * is initialized.
     *
     * <p>Uses a {@link ProfileManager.Observer} instead of {@code
     * profileProviderSupplier.runSyncOrOnAvailable()} so prewarming runs synchronously when the
     * profile is added, rather than being posted to the UI thread by {@link OneshotSupplier}.
     *
     * @param lifecycleDispatcher Provides lifecycle events for the host activity.
     * @param windowAndroid The {@link WindowAndroid} of the host activity.
     * @param profileProviderSupplier Supplier for the activity's {@link ProfileProvider}.
     * @param supportedProfileType The {@link SupportedProfileType} of the host activity.
     * @param isTablet Whether the host activity is in tablet or desktop mode.
     * @param activityCreationUptimeMs The {@code SystemClock.uptimeMillis()} timestamp when the
     *     host activity was created.
     */
    public static void maybePreload(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @Nullable WindowAndroid windowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @SupportedProfileType int supportedProfileType,
            boolean isTablet,
            long activityCreationUptimeMs) {
        if (windowAndroid == null || !isTablet) {
            return;
        }
        // On browser tests, native might not be initialized yet (crbug.com/454692653).
        // onProfileManagerReady() checks the flag again.
        if (FeatureList.isNativeInitialized() && !NtbWebUiCoordinator.isEnabled()) {
            return;
        }
        new WebUiPreloadManager(
                lifecycleDispatcher,
                windowAndroid,
                profileProviderSupplier,
                supportedProfileType,
                activityCreationUptimeMs);
    }

    private WebUiPreloadManager(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WindowAndroid windowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @SupportedProfileType int supportedProfileType,
            long activityCreationUptimeMs) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        mProfileProviderSupplier = profileProviderSupplier;
        mSupportedProfileType = supportedProfileType;
        mActivityCreationUptimeMs = activityCreationUptimeMs;

        if (ProfileManager.isInitialized()) {
            onProfileManagerReady();
        } else {
            mLifecycleDispatcher.register(this);
            ProfileManager.addObserver(this);
        }
    }

    @Override
    public void onProfileAdded(Profile profile) {
        assert ProfileManager.isInitialized();
        ProfileManager.removeObserver(this);
        mLifecycleDispatcher.unregister(this);
        onProfileManagerReady();
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    @Override
    public void onDestroy() {
        ProfileManager.removeObserver(this);
        mLifecycleDispatcher.unregister(this);
    }

    private void onProfileManagerReady() {
        if (mLifecycleDispatcher.isActivityFinishingOrDestroyed()) return;
        if (!NtbWebUiCoordinator.isEnabled()) {
            return;
        }
        ProfileProvider profileProvider = mProfileProviderSupplier.get();
        if (profileProvider == null) return;
        Profile profile =
                switch (mSupportedProfileType) {
                    case SupportedProfileType.REGULAR -> profileProvider.getOriginalProfile();
                    case SupportedProfileType.OFF_THE_RECORD ->
                            profileProvider.getOrCreateOffTheRecordProfile();
                    default -> null;
                };
        if (profile == null) return;
        NtbWebUiCoordinator.prewarm(mWindowAndroid, profile, mActivityCreationUptimeMs);
    }
}
