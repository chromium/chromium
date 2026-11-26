// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Installs the native {@code ChromeWindowColorProviderDelegate} on a {@link ChromeActivity}'s
 * {@link WindowAndroid} once profiles are initialized.
 *
 * <p>Native {@code WindowAndroid} owns the C++ delegate directly; this class holds no raw native
 * pointer and unregisters itself as soon as the delegate is attached (night mode changes recreate
 * the Activity and its {@link WindowAndroid}).
 *
 * <p>Observes {@link ProfileManager} directly (mirroring {@code WebUiPreloadManager}) so the
 * delegate is attached synchronously as soon as profiles are initialized and before top-chrome
 * WebUI prewarming begins.
 */
@NullMarked
public class ChromeWindowColorProviderDelegate implements ProfileManager.Observer, DestroyObserver {
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final NightModeStateProvider mNightModeStateProvider;
    private final boolean mShouldApplyBrowserTheme;
    private final boolean mIsIncognitoWindow;

    /**
     * Attaches a one-shot {@link ChromeWindowColorProviderDelegate} tied to the activity's
     * lifecycle.
     */
    public static void attach(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WindowAndroid windowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            NightModeStateProvider nightModeStateProvider,
            boolean shouldApplyBrowserTheme,
            boolean isIncognitoWindow) {
        new ChromeWindowColorProviderDelegate(
                lifecycleDispatcher,
                windowAndroid,
                profileProviderSupplier,
                nightModeStateProvider,
                shouldApplyBrowserTheme,
                isIncognitoWindow);
    }

    @VisibleForTesting
    ChromeWindowColorProviderDelegate(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WindowAndroid windowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            NightModeStateProvider nightModeStateProvider,
            boolean shouldApplyBrowserTheme,
            boolean isIncognitoWindow) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        mProfileProviderSupplier = profileProviderSupplier;
        mNightModeStateProvider = nightModeStateProvider;
        mShouldApplyBrowserTheme = shouldApplyBrowserTheme;
        mIsIncognitoWindow = isIncognitoWindow;

        if (ProfileManager.isInitialized()) {
            attachToWindow();
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
        attachToWindow();
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    @Override
    public void onDestroy() {
        ProfileManager.removeObserver(this);
        mLifecycleDispatcher.unregister(this);
    }

    private void attachToWindow() {
        if (mLifecycleDispatcher.isActivityFinishingOrDestroyed() || mWindowAndroid.isDestroyed()) {
            return;
        }
        Profile profile = null;
        if (mShouldApplyBrowserTheme && !mIsIncognitoWindow) {
            ProfileProvider profileProvider = mProfileProviderSupplier.get();
            if (profileProvider == null) {
                mProfileProviderSupplier.onAvailable((provider) -> attachToWindow());
                return;
            }
            profile = profileProvider.getOriginalProfile();
        }
        ChromeWindowColorProviderDelegateJni.get()
                .attach(
                        mWindowAndroid,
                        profile,
                        mIsIncognitoWindow,
                        mNightModeStateProvider.isInNightMode());
    }

    @NativeMethods
    interface Natives {
        void attach(
                @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
                @JniType("Profile*") @Nullable Profile profile,
                boolean isIncognitoWindow,
                boolean isNightMode);
    }
}
