// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link CustomTabFeatureOverridesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.CCT_INTENT_FEATURE_OVERRIDES)
public class CustomTabFeatureOverridesManagerUnitTest {
    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();

    private static final Set<String> ALLOWED_FEATURES =
            new HashSet<>(Arrays.asList("ThisFeature", "ThatFeature"));

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;

    @Before
    public void setUp() {
        CustomTabFeatureOverridesManager.setAllowedFeaturesForTesting(ALLOWED_FEATURES);
    }

    @Test
    public void testOverrideEnabled() {
        var list = new ArrayList<>(List.of("ThisFeature", "ThatFeature"));
        setUpIntentWithFeatures(list, null);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertTrue(manager.isFeatureEnabled("ThisFeature"));
        assertTrue(manager.isFeatureEnabled("ThatFeature"));
    }

    @Test
    public void testOverrideDisabled() {
        var list = new ArrayList<>(List.of("ThisFeature", "ThatFeature"));
        setUpIntentWithFeatures(null, list);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertFalse(manager.isFeatureEnabled("ThisFeature"));
        assertFalse(manager.isFeatureEnabled("ThatFeature"));
    }

    @Test
    public void testOverrideEnabledAndDisabled() {
        var enableList = new ArrayList<>(List.of("ThisFeature"));
        var disableList = new ArrayList<>(List.of("ThatFeature"));
        setUpIntentWithFeatures(enableList, disableList);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertTrue(manager.isFeatureEnabled("ThisFeature"));
        assertFalse(manager.isFeatureEnabled("ThatFeature"));
    }

    @Test
    public void testNotOverridden() {
        var list = new ArrayList<>(List.of("ThisFeature"));
        setUpIntentWithFeatures(list, null);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertNull(manager.isFeatureEnabled("ThatFeature"));
    }

    @Test
    public void testOverrideNotAllowed() {
        var list = new ArrayList<>(List.of("OtherFeature"));
        setUpIntentWithFeatures(list, null);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertNull(manager.isFeatureEnabled("OtherFeature"));
    }

    @Test
    public void testOverrideConflict() {
        var enableList = new ArrayList<>(List.of("ThatFeature"));
        var disableList = new ArrayList<>(List.of("ThatFeature"));
        setUpIntentWithFeatures(enableList, disableList);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertNull(manager.isFeatureEnabled("ThatFeature"));
    }

    @Test
    public void testNotTrustedIntent() {
        var enableList = new ArrayList<>(List.of("ThisFeature"));
        var disableList = new ArrayList<>(List.of("ThatFeature"));
        setUpIntentWithFeatures(enableList, disableList);
        when(mIntentDataProvider.isTrustedIntent()).thenReturn(false);
        var manager = new CustomTabFeatureOverridesManager(mIntentDataProvider);

        assertNull(manager.isFeatureEnabled("ThisFeature"));
        assertNull(manager.isFeatureEnabled("ThatFeature"));
    }

    private void setUpIntentWithFeatures(ArrayList<String> enabled, ArrayList<String> disabled) {
        var intent = new CustomTabsIntent.Builder().build().intent;
        if (enabled != null) {
            intent.putExtra(CustomTabIntentDataProvider.EXPERIMENTS_ENABLE, enabled);
        }
        if (disabled != null) {
            intent.putExtra(CustomTabIntentDataProvider.EXPERIMENTS_DISABLE, disabled);
        }
        when(mIntentDataProvider.getIntent()).thenReturn(intent);
        when(mIntentDataProvider.isTrustedIntent()).thenReturn(true);
    }
}
