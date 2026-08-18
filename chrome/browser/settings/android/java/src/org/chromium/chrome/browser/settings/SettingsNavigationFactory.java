// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

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
     * <p>If SettingsInTab and URL navigation are enabled and a valid activity context is provided,
     * resolves the tab-scoped delegate bound to the active {@link SettingsHostFragment}.
     */
    public static SettingsNavigation createSettingsNavigation(Context context) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        if (!SettingsInTab.isEnabled() || !ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
            return sInstance;
        }

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
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
}
