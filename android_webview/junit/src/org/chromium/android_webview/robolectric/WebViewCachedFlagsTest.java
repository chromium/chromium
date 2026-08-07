// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.chromium.android_webview.common.WebViewCachedFlags.param;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.build.BuildConfig;

import java.util.Map;
import java.util.Set;

/** Tests for WebViewCachedFlags. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebViewCachedFlagsTest {
    // Keep these in sync with the prefs/histogram names in WebViewCachedFlags.java.
    private static final String CACHED_ENABLED_FLAGS_PREF = "CachedFlagsEnabled";
    private static final String CACHED_DISABLED_FLAGS_PREF = "CachedFlagsDisabled";
    private static final String CACHED_PARAMS_PREF = "CachedParams";
    private static final String CACHED_FLAGS_EXIST_HISTOGRAM_NAME =
            "Android.WebView.CachedFlagsExist";

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void doubleInitFailsAssertion() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        WebViewCachedFlags.init(sharedPrefs);
        try {
            Assert.assertThrows(AssertionError.class, () -> WebViewCachedFlags.init(sharedPrefs));
        } finally {
            WebViewCachedFlags.resetForTesting();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void sharedPrefsAreDeletedOnInit() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of("Foo", "Bar"))
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of("Baz"))
                .apply();
        new WebViewCachedFlags(sharedPrefs, Map.of(), Map.of(), false);
        Assert.assertFalse(sharedPrefs.contains(CACHED_ENABLED_FLAGS_PREF));
        Assert.assertFalse(sharedPrefs.contains(CACHED_DISABLED_FLAGS_PREF));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void flagsShowAsEnabledAndDisabledCorrectly() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of("Foo", "Bar"))
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of("Baz"))
                .apply();
        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(sharedPrefs, Map.of(), Map.of(), false);
        Assert.assertTrue(cachedFlags.isCachedFeatureEnabled("Foo"));
        Assert.assertTrue(cachedFlags.isCachedFeatureEnabled("Bar"));
        Assert.assertFalse(cachedFlags.isCachedFeatureEnabled("Baz"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void flagsDefaultToCorrectValueWhenNotCached() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of())
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of())
                .apply();
        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.DISABLED,
                                "Bar", WebViewCachedFlags.DefaultState.DISABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.of(),
                        false);
        Assert.assertFalse(cachedFlags.isCachedFeatureEnabled("Foo"));
        Assert.assertFalse(cachedFlags.isCachedFeatureEnabled("Bar"));
        Assert.assertTrue(cachedFlags.isCachedFeatureEnabled("Baz"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({"Baz"})
    @Features.DisableFeatures({"Foo", "Bar"})
    public void flagsAreSetCorrectlyPostStartup() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of("Foo", "Bar"))
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of("Baz"))
                .apply();
        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.ENABLED,
                                "Bar", WebViewCachedFlags.DefaultState.ENABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.of(),
                        false);

        cachedFlags.onStartupCompleted(sharedPrefs);
        Assert.assertEquals(
                Set.of("Baz"), sharedPrefs.getStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of()));
        Assert.assertEquals(
                Set.of("Foo", "Bar"),
                sharedPrefs.getStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of()));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void logWhetherCachedFlagsExist() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of())
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of())
                .apply();
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(CACHED_FLAGS_EXIST_HISTOGRAM_NAME, true)) {
            new WebViewCachedFlags(sharedPrefs, Map.of(), Map.of(), false);
        }

        InMemorySharedPreferences emptySharedPrefs = new InMemorySharedPreferences();
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(CACHED_FLAGS_EXIST_HISTOGRAM_NAME, false)) {
            new WebViewCachedFlags(emptySharedPrefs, Map.of(), Map.of(), false);
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({"Baz"})
    @Features.DisableFeatures({"Foo"})
    public void flagsAreNotSetIfNotOverridden() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.ENABLED,
                                "Bar", WebViewCachedFlags.DefaultState.ENABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.of(),
                        false);

        cachedFlags.onStartupCompleted(sharedPrefs);
        Assert.assertEquals(
                Set.of("Baz"), sharedPrefs.getStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of()));
        Assert.assertEquals(
                Set.of("Foo"), sharedPrefs.getStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of()));

        // Simulate another startup
        WebViewCachedFlags newCachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.ENABLED,
                                "Bar", WebViewCachedFlags.DefaultState.ENABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.of(),
                        false);
        Assert.assertTrue(newCachedFlags.isCachedFeatureOverridden("Baz"));
        Assert.assertTrue(newCachedFlags.isCachedFeatureOverridden("Foo"));
        Assert.assertFalse(newCachedFlags.isCachedFeatureOverridden("Bar"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testFeatureAccessHistograms() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.DISABLED,
                                "Bar", WebViewCachedFlags.DefaultState.DISABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED,
                                "Back", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.of(),
                        false);

        int fooHash = WebViewCachedFlags.hashFieldTrialName("Foo");
        int barHash = WebViewCachedFlags.hashFieldTrialName("Bar");
        int backHash = WebViewCachedFlags.hashFieldTrialName("Back");

        // Before startup is completed, both histograms should be logged for Foo.
        try (HistogramWatcher ignoredEarly =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Variations.FeatureAccess", fooHash)
                        .expectIntRecord("Variations.FeatureAccessEarly", fooHash)
                        .build()) {
            cachedFlags.isCachedFeatureEnabled("Foo");
            // Calling it a second time should not log the histogram again.
            cachedFlags.isCachedFeatureEnabled("Foo");
        }

        // After startup is completed, only Variations.FeatureAccess is logged for Bar.
        cachedFlags.onStartupCompleted(sharedPrefs);
        try (HistogramWatcher ignoredLate =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Variations.FeatureAccess", barHash)
                        .expectNoRecords("Variations.FeatureAccessEarly")
                        .build()) {
            cachedFlags.isCachedFeatureEnabled("Bar");
            // Calling it a second time should not log the histogram again.
            cachedFlags.isCachedFeatureEnabled("Bar");
        }

        // Check that a negative hash (e.g. for "Back") is logged correctly.
        try (HistogramWatcher ignoredNegative =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Variations.FeatureAccess", backHash)
                        .expectNoRecords("Variations.FeatureAccessEarly")
                        .build()) {
            cachedFlags.isCachedFeatureEnabled("Back");
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testHashFieldTrialName() {
        Assert.assertEquals(0x3f66c0bc, WebViewCachedFlags.hashFieldTrialName("NewTab"));
        Assert.assertEquals(0x26724eba, WebViewCachedFlags.hashFieldTrialName("Forward"));
        Assert.assertEquals(0xb7362bb5, WebViewCachedFlags.hashFieldTrialName("Back"));
        // C++ base::HashFieldTrialName() returns a uint32_t. Java does not have an unsigned 32-bit
        // type, so we use a signed int. The "Back" test case demonstrates that values > 0x7FFFFFFF
        // (the maximum signed int) are correctly represented as negative numbers in Java.
        Assert.assertEquals(-1221186635, WebViewCachedFlags.hashFieldTrialName("Back"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testForceDefaults() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, Set.of("Foo", "Bar"))
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, Set.of("Baz"))
                .putStringSet(
                        CACHED_PARAMS_PREF,
                        Set.of("TestFeature:str_param:a:b:c", "TestFeature:int_param:42"))
                .apply();

        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(
                                "Foo", WebViewCachedFlags.DefaultState.DISABLED,
                                "Bar", WebViewCachedFlags.DefaultState.DISABLED,
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED),
                        Map.ofEntries(
                                param("TestFeature", "str_param", "default_str"),
                                param("TestFeature", "int_param", "99")),
                        true);
        // Should have default values.
        Assert.assertFalse(cachedFlags.isCachedFeatureEnabled("Foo"));
        Assert.assertFalse(cachedFlags.isCachedFeatureEnabled("Bar"));
        Assert.assertTrue(cachedFlags.isCachedFeatureEnabled("Baz"));

        // Should have default parameter values.
        Assert.assertEquals(
                "default_str",
                cachedFlags.getCachedFeatureParamAsString("TestFeature", "str_param"));
        Assert.assertEquals(99, cachedFlags.getCachedFeatureParamAsInt("TestFeature", "int_param"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testParameterCachingAndCrashProtection() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putStringSet(
                        CACHED_PARAMS_PREF,
                        Set.of(
                                "TestFeature:int_param:42",
                                "TestFeature:bool_param:true",
                                "TestFeature:double_param:3.14",
                                "TestFeature:str_param:a:b:c"))
                .apply();

        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(),
                        Map.ofEntries(
                                param("TestFeature", "int_param", "100"),
                                param("TestFeature", "bool_param", "false"),
                                param("TestFeature", "double_param", "2.71"),
                                param("TestFeature", "str_param", "world"),
                                param("TestFeature", "non_existent", "99")),
                        false);

        // Verify reading returns the cached values perfectly.
        Assert.assertEquals(42, cachedFlags.getCachedFeatureParamAsInt("TestFeature", "int_param"));
        Assert.assertTrue(cachedFlags.getCachedFeatureParamAsBoolean("TestFeature", "bool_param"));
        Assert.assertEquals(
                3.14,
                cachedFlags.getCachedFeatureParamAsDouble("TestFeature", "double_param"),
                0.001);
        Assert.assertEquals(
                "a:b:c", cachedFlags.getCachedFeatureParamAsString("TestFeature", "str_param"));

        // Verify crash protection: the prefs should have been deleted immediately.
        Assert.assertFalse(sharedPrefs.contains(CACHED_PARAMS_PREF));

        // Verify that querying a non-existent parameter returns the default value.
        Assert.assertEquals(
                99, cachedFlags.getCachedFeatureParamAsInt("TestFeature", "non_existent"));

        // Verify that querying an unregistered parameter throws an IllegalArgumentException.
        Assert.assertThrows(
                IllegalArgumentException.class,
                () -> cachedFlags.getCachedFeatureParamAsInt("TestFeature", "unregistered"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({"TestFeature:active_param/override_hello"})
    public void testOnStartupCompletedDropsStaleParamsFromPrefs() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        // Simulate a stale parameters file and an unrelated key
        sharedPrefs
                .edit()
                .putStringSet(
                        CACHED_PARAMS_PREF,
                        Set.of("DeletedFeature:stale_param:bye", "TestFeature:active_param:hello"))
                .putString("SomeOtherUnrelatedKey", "keep_me")
                .apply();

        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(),
                        Map.ofEntries(param("TestFeature", "active_param", "world")),
                        false);
        cachedFlags.onStartupCompleted(sharedPrefs);

        Set<String> cachedParams = sharedPrefs.getStringSet(CACHED_PARAMS_PREF, Set.of());
        Assert.assertEquals(Set.of("TestFeature:active_param:override_hello"), cachedParams);
        // Verify that unrelated keys in the shared preferences file are left untouched.
        Assert.assertTrue(sharedPrefs.contains("SomeOtherUnrelatedKey"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({"TestFeature:paramA/overrideA/paramB/overrideB"})
    public void testOnStartupCompletedUpdatesParamsInPrefs() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();

        WebViewCachedFlags cachedFlags =
                new WebViewCachedFlags(
                        sharedPrefs,
                        Map.of(),
                        Map.ofEntries(
                                param("TestFeature", "paramA", "defaultA"),
                                param("TestFeature", "paramB", "defaultB"),
                                param("UnrelatedFeature", "paramC", "defaultC")),
                        false);
        cachedFlags.onStartupCompleted(sharedPrefs);

        Set<String> cachedParams = sharedPrefs.getStringSet(CACHED_PARAMS_PREF, Set.of());
        Assert.assertEquals(
                Set.of("TestFeature:paramA:overrideA", "TestFeature:paramB:overrideB"),
                cachedParams);
    }
}
