// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ReducedModeNativeTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/** Integration tests for {@link PrefetchConfiguration}. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class PrefetchConfigurationTest {
    @Rule
    public ReducedModeNativeTestRule mNativeTestRule = new ReducedModeNativeTestRule();

    @Test
    @MediumTest
    @Feature("OfflinePrefetch")
    @Features.EnableFeatures(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING)
    public void testWithPrefetchingFeatureEnabled() {
        AtomicBoolean isFlagEnabled = new AtomicBoolean();
        AtomicBoolean isEnabled = new AtomicBoolean();
        OfflineTestUtil.setPrefetchingEnabledByServer(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertTrue(isFlagEnabled.get());
        assertTrue(isEnabled.get());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Disable prefetching user setting.
            PrefetchConfiguration.setPrefetchingEnabledInSettings(false);
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertTrue(isFlagEnabled.get());
        assertFalse(isEnabled.get());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-enable prefetching user setting.
            PrefetchConfiguration.setPrefetchingEnabledInSettings(true);
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertTrue(isFlagEnabled.get());
        assertTrue(isEnabled.get());
    }

    @Test
    @MediumTest
    @Feature("OfflinePrefetch")
    @Features.DisableFeatures(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING)
    public void testWithPrefetchingFeatureDisabled() {
        AtomicBoolean isFlagEnabled = new AtomicBoolean();
        AtomicBoolean isEnabled = new AtomicBoolean();
        OfflineTestUtil.setPrefetchingEnabledByServer(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertFalse(isFlagEnabled.get());
        assertFalse(isEnabled.get());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Disable prefetching user setting.
            PrefetchConfiguration.setPrefetchingEnabledInSettings(false);
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertFalse(isFlagEnabled.get());
        assertFalse(isEnabled.get());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-enable prefetching user setting.
            PrefetchConfiguration.setPrefetchingEnabledInSettings(true);
            isFlagEnabled.set(PrefetchConfiguration.isPrefetchingFlagEnabled());
            isEnabled.set(PrefetchConfiguration.isPrefetchingEnabled());
        });
        assertFalse(isFlagEnabled.get());
        assertFalse(isEnabled.get());
    }
}
