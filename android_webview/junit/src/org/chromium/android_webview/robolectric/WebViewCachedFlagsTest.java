// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import androidx.test.filters.SmallTest;

import com.android.webview.chromium.WebViewCachedFlags;

import org.chromium.android_webview.common.AwFeatures;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.InMemorySharedPreferences;

import java.util.Map;
import java.util.Set;

/** Tests for WebViewCachedFlags. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebViewCachedFlagsTest {
    // Keep these in sync with the prefs names in WebViewCachedFlags.java.
    private static final String CACHED_ENABLED_FLAGS_PREF = "CachedFlagsEnabled";
    private static final String CACHED_DISABLED_FLAGS_PREF = "CachedFlagsDisabled";

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
        new WebViewCachedFlags(sharedPrefs, Map.of());
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
        WebViewCachedFlags cachedFlags = new WebViewCachedFlags(sharedPrefs, Map.of());
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
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED));
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
                                "Baz", WebViewCachedFlags.DefaultState.ENABLED));

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
    public void manualFlagsAreMigrated() {
        InMemorySharedPreferences sharedPrefs = new InMemorySharedPreferences();
        sharedPrefs
                .edit()
                .putBoolean("useWebViewResourceContext", true)
                .putBoolean("defaultWebViewPartitionedCookiesState", true)
                .putBoolean("webViewUseStartupTasksLogic", true)
                .apply();
        WebViewCachedFlags cachedFlags = new WebViewCachedFlags(sharedPrefs, Map.of());

        // The flags should be enabled if the prefs were present.
        Assert.assertTrue(
                cachedFlags.isCachedFeatureEnabled(AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT));
        Assert.assertTrue(cachedFlags.isCachedFeatureEnabled(AwFeatures.WEBVIEW_DISABLE_CHIPS));
        Assert.assertTrue(
                cachedFlags.isCachedFeatureEnabled(AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC));
        // Check that we removed the old prefs.
        Assert.assertFalse(sharedPrefs.contains("useWebViewResourceContext"));
        Assert.assertFalse(sharedPrefs.contains("defaultWebViewPartitionedCookiesState"));
        Assert.assertFalse(sharedPrefs.contains("webViewUseStartupTasksLogic"));
    }
}
