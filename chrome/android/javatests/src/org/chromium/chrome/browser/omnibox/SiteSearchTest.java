// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.KeyEvent;
import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

import java.util.ArrayList;
import java.util.List;

/** Instrumentation tests for site search on Android Large Form Factor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
@ImportantFormFactors(DeviceFormFactor.TABLET_OR_DESKTOP)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
public class SiteSearchTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final List<String> mKeywordsAdded = new ArrayList<>();
    private ChromeTabbedActivity mActivity;
    private OmniboxTestUtils mOmniboxUtils;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mOmniboxUtils = new OmniboxTestUtils(mActivity);
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);

        // Ensure TemplateUrlService is loaded.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlService service =
                            TemplateUrlServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    service.load();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    TemplateUrlService service =
                            TemplateUrlServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    return service.isLoaded();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlService service =
                            TemplateUrlServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    for (String keyword : mKeywordsAdded) {
                        service.removeSearchEngine(keyword);
                    }
                });
        mKeywordsAdded.clear();
    }

    private void addSearchEngine(String shortName, String keyword, String searchUrl) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlService service =
                            TemplateUrlServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    service.addSearchEngine(shortName, keyword, searchUrl);
                });
        mKeywordsAdded.add(keyword);
    }

    @Test
    @LargeTest
    public void testSiteSearchSuggestionAppears() {
        addSearchEngine("TestName", "test", "https://www.test.com/search?q={searchTerms}");

        // Open Chrome -> select omnibox -> type "test"
        mOmniboxUtils.requestFocus();
        mOmniboxUtils.typeText("test", false);

        // Wait for suggestions to show up
        mOmniboxUtils.checkSuggestionsShown();

        // Verify that the Site Search Action chip/label "Search Test" appears in the suggestion row
        ViewUtils.onViewWaiting(withText("Search TestName")).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testSiteSearchTriggeredByTab() {
        addSearchEngine("TestName", "test", "https://www.test.com/search?q={searchTerms}");

        // Open Chrome -> select omnibox -> type "test"
        mOmniboxUtils.requestFocus();
        mOmniboxUtils.typeText("test", false);
        mOmniboxUtils.checkSuggestionsShown();

        // First row should be selected when typing
        checkIsFirstSuggestionRowSelected();

        // Wait for the Site Search Action chip to be bound and displayed in the horizontal carousel
        ViewUtils.onViewWaiting(withText("Search TestName")).check(matches(isDisplayed()));

        // Press <tab>
        mOmniboxUtils.sendKey(KeyEvent.KEYCODE_TAB);

        checkIsActionChipSelected("Search TestName");

        // Check omnibox becomes "" since the site search is triggered
        mOmniboxUtils.checkText(Matchers.equalTo(""), null);
    }

    private void checkIsFirstSuggestionRowSelected() {
        OmniboxTestUtils.SuggestionInfo<View> firstSuggestion =
                mOmniboxUtils.findSuggestion(info -> true);
        CriteriaHelper.pollUiThread(() -> firstSuggestion.view.isSelected());
    }

    private void checkIsActionChipSelected(String chipText) {
        // Since action chips are dynamically created, we locate it by asserting the view
        // is a ChipView containing specific text, and then verify it has been selected.
        ViewUtils.onViewWaiting(
                        Matchers.allOf(
                                ViewMatchers.isAssignableFrom(ChipView.class),
                                ViewMatchers.hasDescendant(withText(chipText)),
                                ViewMatchers.isSelected()))
                .check(matches(isDisplayed()));
    }
}
