// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/** Unit tests for {@link HubFieldTrial}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubFieldTrialTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testHubEnabled() {
        assertTrue(HubFieldTrial.isHubEnabled());
    }

    @Test
    @SmallTest
    public void testHubDisabled() {
        assertFalse(HubFieldTrial.isHubEnabled());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testUsesFloatActionButton() {
        assertFalse(HubFieldTrial.usesFloatActionButton());
        HubFieldTrial.FLOATING_ACTION_BUTTON.setForTesting(true);
        assertTrue(HubFieldTrial.usesFloatActionButton());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testDoesPaneSwitcherUseText() {
        assertFalse(HubFieldTrial.doesPaneSwitcherUseText());
        HubFieldTrial.PANE_SWITCHER_USES_TEXT.setForTesting(true);
        assertTrue(HubFieldTrial.doesPaneSwitcherUseText());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testSupportsOtherTabs() {
        assertFalse(HubFieldTrial.supportsOtherTabs());
        HubFieldTrial.SUPPORTS_OTHER_TABS.setForTesting(true);
        assertTrue(HubFieldTrial.supportsOtherTabs());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testSupportsBookmarks() {
        assertFalse(HubFieldTrial.supportsBookmarks());
        HubFieldTrial.SUPPORTS_BOOKMARKS.setForTesting(true);
        assertTrue(HubFieldTrial.supportsBookmarks());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_HUB})
    public void testSupportsSearch() {
        assertFalse(HubFieldTrial.supportsSearch());
        HubFieldTrial.SUPPORTS_SEARCH.setForTesting(true);
        assertTrue(HubFieldTrial.supportsSearch());
    }
}
