// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;

/**
 * Tests functionality related to TabContext
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSuggestionTest {
    private static final @TabSuggestion.TabSuggestionAction int TAB_SUGGESTION_ACTION =
            TabSuggestion.TabSuggestionAction.CLOSE;
    private static final String PROVIDER_NAME = "providerName";
    private static final int TAB_GROUP_ID = 1;
    private static final int ID = 1;
    private static final String TITLE = "title";
    private static final String TAB_URL = "url";
    private static final String ORIGINAL_URL = "original_url";
    private static final String REFERRER_URL = "referrer_url";
    private static final long TIMESTAMP = 4352345L;
    private static final String VISIBLE_URL = "visible_url";
    private static final TabContext.TabInfo TAB_INFO = new TabContext.TabInfo(
            ID, TITLE, TAB_URL, ORIGINAL_URL, REFERRER_URL, TIMESTAMP, VISIBLE_URL);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testNonEmptySuggestions() {
        TabSuggestion tabSuggestion = new TabSuggestion(
                Arrays.asList(TAB_INFO), TAB_SUGGESTION_ACTION, PROVIDER_NAME, TAB_GROUP_ID);
        Assert.assertNotNull(tabSuggestion.getTabsInfo());
        Assert.assertEquals(tabSuggestion.getTabsInfo().size(), 1);
        Assert.assertEquals(tabSuggestion.getTabsInfo().get(0), TAB_INFO);
    }

    @Test
    public void testNullSuggestions() {
        TabSuggestion tabSuggestion =
                new TabSuggestion(null, TAB_SUGGESTION_ACTION, PROVIDER_NAME, TAB_GROUP_ID);
        Assert.assertNotNull(tabSuggestion.getTabsInfo());
        Assert.assertEquals(tabSuggestion.getTabsInfo().size(), 0);
    }
}
