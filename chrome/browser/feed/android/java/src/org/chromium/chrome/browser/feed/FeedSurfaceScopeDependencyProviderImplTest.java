// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;

/**
 * Tests for FeedSurfaceScopeDependencyProviderImpl. Uses ChromeTabbedActivityTestRule to test
 * native code.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.INTEREST_FEED_V2)
public class FeedSurfaceScopeDependencyProviderImplTest {
    static final byte[] VALUE_1 = "one".getBytes();
    static final byte[] VALUE_2 = "two".getBytes();

    String toString(byte[] array) {
        if (array == null) return "null";
        return new String(array);
    }

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL("about:blank");
    }

    @Test
    @MediumTest
    @Feature({"Feed"})
    public void testPersistentKeyValueCachePutAndLookup() {
        FeedSurfaceScopeDependencyProviderImpl dependencyProvider =
                new FeedSurfaceScopeDependencyProviderImpl(
                        /* activity= */ null, /* activityContext= */ null, /* darkMode= */ false);
        ArrayList<String> calls = new ArrayList<String>();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PersistentKeyValueCache cache = dependencyProvider.getPersistentKeyValueCache();
            cache.put(VALUE_1, VALUE_2, () -> calls.add("put"));
            cache.lookup(VALUE_1, (byte[] v) -> calls.add("lookup1 " + toString(v)));
            cache.lookup(VALUE_2, (byte[] v) -> calls.add("lookup2 " + toString(v)));
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "Calls match", calls, Matchers.contains("put", "lookup1 two", "lookup2 null"));
        });
    }

    @Test
    @MediumTest
    @Feature({"Feed"})
    public void testPersistentKeyValueCacheEvict() {
        FeedSurfaceScopeDependencyProviderImpl dependencyProvider =
                new FeedSurfaceScopeDependencyProviderImpl(
                        /* activity= */ null, /* activityContext= */ null, /* darkMode= */ false);
        ArrayList<String> calls = new ArrayList<String>();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PersistentKeyValueCache cache = dependencyProvider.getPersistentKeyValueCache();
            cache.put(VALUE_1, VALUE_2, () -> calls.add("put"));
            cache.evict(VALUE_1, () -> calls.add("evict"));
            cache.lookup(VALUE_1, (byte[] v) -> calls.add("lookup " + toString(v)));
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "Calls match", calls, Matchers.contains("put", "evict", "lookup null"));
        });
    }

    @Test
    @MediumTest
    @Feature({"Feed"})
    public void testPersistentKeyValueCacheNullRunnables() {
        // Verify put() and evict() accept null runnables.
        FeedSurfaceScopeDependencyProviderImpl dependencyProvider =
                new FeedSurfaceScopeDependencyProviderImpl(
                        /* activity= */ null, /* activityContext= */ null, /* darkMode= */ false);
        ArrayList<String> calls = new ArrayList<String>();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PersistentKeyValueCache cache = dependencyProvider.getPersistentKeyValueCache();
            cache.put(VALUE_1, VALUE_2, null);
            cache.evict(VALUE_1, null);
            cache.put(VALUE_1, VALUE_2, () -> calls.add("put"));
        });

        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat("Calls match", calls, Matchers.contains("put")); });
    }
}
