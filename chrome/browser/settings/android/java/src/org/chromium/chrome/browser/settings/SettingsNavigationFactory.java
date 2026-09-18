// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.ui.base.WindowAndroid;

/** Factory for {@link SettingsNavigation}. Can be used from chrome/browser modules. */
@NullMarked
public final class SettingsNavigationFactory {
    private static final SettingsNavigation sInstance = new SettingsNavigationImpl();
    private static @Nullable SettingsNavigation sInstanceForTesting;

    private SettingsNavigationFactory() {}

    /** Create a default {@link SettingsNavigation} instance. */
    public static SettingsNavigation createSettingsNavigation() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sInstance;
    }

    /**
     * Create a {@link SettingsNavigation} instance scoped to the tab holding the given context.
     *
     * <p>If URL navigation is enabled and a valid activity context is provided, resolves the
     * tab-scoped delegate bound to the active {@link SettingsHostFragment}.
     */
    public static SettingsNavigation createSettingsNavigation(Context context) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        if (!ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
            return sInstance;
        }

        // SettingsInTabUrlNav implies that SettingsInTab is enabled.
        assert SettingsInTab.isFeatureEnabled();

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return sInstance;
        }

        SettingsHostFragment hostFragment = SettingsHostFragment.get(activity);
        if (hostFragment == null || hostFragment.getSettingsNavigation() == null) {
            return sInstance;
        }

        return hostFragment.getSettingsNavigation();
    }

    /** Set a test double to replace the real {@link SettingsNavigationImpl} in a test. */
    public static void setInstanceForTesting(SettingsNavigation instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /**
     * Open Clear Browsing Data settings from native WebUI.
     *
     * @param windowAndroid The window associated with the WebContents.
     */
    @CalledByNative
    public static void showClearBrowsingData(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            return;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return;
        }
        createSettingsNavigation(activity)
                .startSettings(activity, SettingsFragment.CLEAR_BROWSING_DATA);
    }
}
