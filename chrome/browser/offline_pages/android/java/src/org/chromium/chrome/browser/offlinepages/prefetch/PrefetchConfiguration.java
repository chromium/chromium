// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileKey;

/**
 * Allows the querying and setting of Offline Prefetch related configurations.
 */
@JNINamespace("offline_pages::android")
public class PrefetchConfiguration {
    /**
     * Returns true if the Offline Prefetch feature flag is enabled. This can be used to determine
     * if any evidence of this feature should be presented to the user (like its respective user
     * settings entries).
     */
    public static boolean isPrefetchingFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING);
    }

    /**
     * Returns true if Offline Prefetch is allowed to run, requiring both the feature flag and the
     * user setting to be true. If the current browser Profile is null this method returns false.
     */
    public static boolean isPrefetchingEnabled(ProfileKey profileKey) {
        return PrefetchConfigurationJni.get().isPrefetchingEnabled(profileKey);
    }

    /**
     * Return the value of offline_pages.enabled_by_server pref.
     */
    public static boolean isPrefetchingEnabledByServer(ProfileKey profileKey) {
        return PrefetchConfigurationJni.get().isEnabledByServer(profileKey);
    }

    /**
     * Returns true if it's time for a GeneratePageBundle-forbidden check. This could be the case
     * if the check has never run before or we are forbidden and it's been more than seven days
     * since the last check.
     */
    public static boolean isForbiddenCheckDue(ProfileKey profileKey) {
        return PrefetchConfigurationJni.get().isForbiddenCheckDue(profileKey);
    }

    /**
     * Returns true if the GeneratePageBundle-forbidden check has never run and is due to run.
     */
    public static boolean isEnabledByServerUnknown(ProfileKey profileKey) {
        return PrefetchConfigurationJni.get().isEnabledByServerUnknown(profileKey);
    }

    /**
     * Sets the value of the user controlled setting that controls whether Offline Prefetch is
     * enabled or disabled. If the current browser Profile is null the setting will not be changed.
     */
    public static void setPrefetchingEnabledInSettings(ProfileKey profileKey, boolean enabled) {
        PrefetchConfigurationJni.get().setPrefetchingEnabledInSettings(profileKey, enabled);
    }

    /**
     * Gets the value of the user controlled setting that controls whether Offline Prefetch is
     * enabled or disabled.
     */
    public static boolean isPrefetchingEnabledInSettings(ProfileKey profileKey) {
        return PrefetchConfigurationJni.get().isPrefetchingEnabledInSettings(profileKey);
    }

    @NativeMethods
    interface Natives {
        boolean isPrefetchingEnabled(ProfileKey key);
        boolean isEnabledByServer(ProfileKey key);
        boolean isForbiddenCheckDue(ProfileKey key);
        boolean isEnabledByServerUnknown(ProfileKey key);
        void setPrefetchingEnabledInSettings(ProfileKey key, boolean enabled);
        boolean isPrefetchingEnabledInSettings(ProfileKey key);
    }
}
