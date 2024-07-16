// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocalTabTabResumptionDataProviderUnitTest extends TestSupport {
    private int mFetchSuggestionsCallbackCounter;

    @Test
    @SmallTest
    public void testNullTab() {
        LocalTabTabResumptionDataProvider localTabProvider =
                new LocalTabTabResumptionDataProvider(null);
        localTabProvider.fetchSuggestions(
                (SuggestionsResult results) -> {
                    Assert.assertEquals(results.strength, ResultStrength.FORCED_NULL);
                    Assert.assertNull(results.suggestions);
                    ++mFetchSuggestionsCallbackCounter;
                });
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
    }

    @Test
    @SmallTest
    public void testOneTab() {
        Tab tab = makeMockBrowserTab();
        LocalTabTabResumptionDataProvider localTabProvider =
                new LocalTabTabResumptionDataProvider(tab);
        localTabProvider.fetchSuggestions(
                (SuggestionsResult results) -> {
                    Assert.assertEquals(results.strength, ResultStrength.STABLE);
                    Assert.assertEquals(1, results.suggestions.size());
                    SuggestionEntry entry1 = results.suggestions.get(0);
                    Assert.assertTrue(entry1.isLocalTab());
                    Assert.assertEquals(JUnitTestGURLs.BLUE_1, entry1.url);
                    Assert.assertEquals("Blue 1", entry1.title);
                    ++mFetchSuggestionsCallbackCounter;
                });
        Assert.assertEquals(1, mFetchSuggestionsCallbackCounter);
    }
}
