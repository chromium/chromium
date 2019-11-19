// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Misc. Content Suggestions instrumentation tests.
 */
// TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class ContentSuggestionsTest {
    // clang-format on

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Feature("Suggestions")
    public void testRemoteSuggestionsEnabled() {
        NewTabPage ntp = loadNTPWithSearchSuggestState(true);
        SuggestionsUiDelegate uiDelegate = ntp.getManagerForTesting();
        Assert.assertTrue(
                isCategoryEnabled(uiDelegate.getSuggestionsSource(), KnownCategories.ARTICLES));
    }

    @Test
    @SmallTest
    @Feature("Suggestions")
    public void testRemoteSuggestionsDisabled() {
        NewTabPage ntp = loadNTPWithSearchSuggestState(false);
        SuggestionsUiDelegate uiDelegate = ntp.getManagerForTesting();
        // Since header is expandable, category should still be enabled.
        Assert.assertEquals(true,
                isCategoryEnabled(uiDelegate.getSuggestionsSource(), KnownCategories.ARTICLES));
    }

    private NewTabPage loadNTPWithSearchSuggestState(final boolean enabled) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PrefServiceBridge.getInstance().setBoolean(
                                Pref.SEARCH_SUGGEST_ENABLED, enabled));

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
        return (NewTabPage) tab.getNativePage();
    }

    private boolean isCategoryEnabled(SuggestionsSource source, @CategoryInt int category) {
        int[] categories = source.getCategories();
        for (int cat : categories) {
            if (cat != category) continue;
            if (SnippetsBridge.isCategoryEnabled(source.getCategoryStatus(category))) return true;
        }

        return false;
    }
}
