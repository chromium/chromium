// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Factory for {@link SettingsNavigation}. Can be used from chrome/browser modules. */
@NullMarked
public class SettingsNavigationFactory {
    private static final SettingsNavigation sInstance = new SettingsNavigationImpl();
    private static @Nullable SettingsNavigation sInstanceForTesting;

    /** Create a {@link SettingsNavigation}. */
    public static SettingsNavigation createSettingsNavigation() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sInstance;
    }

    /** Set a test double to replace the real {@link SettingsNavigationImpl} in a test. */
    public static void setInstanceForTesting(SettingsNavigation instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
