// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests the full continuous search UI. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
@Batch(Batch.PER_CLASS)
public class ContinuousSearchFullUiTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private GURL mUrl;

    public ContinuousSearchFullUiTest() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.TRIGGER_MODE_PARAM,
                "0");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.SHOW_RESULT_TITLE_PARAM,
                "true");
        FeatureList.setTestValues(testValues);
    }

    @Before
    public void setUp() {
        mUrl =
                new GURL(
                        sActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/simple.html"));
        sActivityTestRule.loadUrl(mUrl.getSpec());
    }

    /** Assert that all the critical views are shown using mock data to trigger the UI. */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testDisplaysFullUi() throws TimeoutException {
        List<PageItem> results = new ArrayList<PageItem>();
        results.add(new PageItem(new GURL("https://www.foo.com/"), "Foo Result"));
        results.add(new PageItem(new GURL("https://www.bar.com/"), "Bar Result"));
        results.add(new PageItem(mUrl, "Default Result"));
        results.add(new PageItem(new GURL("https://www.baz.com/"), "Baz Result"));
        results.add(new PageItem(new GURL("https://www.example.com/"), "Example Result"));
        results.add(new PageItem(new GURL("https://www.chromium.org/"), "Chromium Result"));
        List<PageGroup> groups = new ArrayList<PageGroup>();
        groups.add(new PageGroup("Group 1", false, results));
        ContinuousNavigationMetadata metadata =
                new ContinuousNavigationMetadata(
                        new GURL("https://www.google.com/search?q=foo"),
                        "foo",
                        new ContinuousNavigationMetadata.Provider(
                                PageCategory.ORGANIC_SRP,
                                "Search",
                                R.drawable.ic_logo_googleg_20dp),
                        groups);

        // Show the container and contents.
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContinuousNavigationUserData.getForTab(tab).updateData(metadata, mUrl);
                });

        // Ensure all the view information is shown. This automatically checks for visible.
        ViewUtils.waitForVisibleView(
                allOf(
                        withParent(withId(R.id.continuous_search_container_stub)),
                        withId(org.chromium.chrome.browser.continuous_search.R.id.container_view)));
        ViewUtils.waitForVisibleView(
                withId(
                        org.chromium.chrome.browser.continuous_search.R.id
                                .continuous_search_provider_label));
        ViewUtils.waitForVisibleView(
                withId(org.chromium.chrome.browser.continuous_search.R.id.button_dismiss));

        // Check the items in the carousel exist.
        RecyclerView carousel =
                (RecyclerView)
                        sActivityTestRule
                                .getActivity()
                                .findViewById(
                                        org.chromium.chrome.browser.continuous_search.R.id
                                                .recycler_view);
        Assert.assertNotNull(carousel);
        Assert.assertEquals(results.size(), carousel.getAdapter().getItemCount());

        // Wait for animated scroll to the open element.
        LinearLayoutManager layoutManager = (LinearLayoutManager) carousel.getLayoutManager();
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                layoutManager.findFirstCompletelyVisibleItemPosition(),
                                Matchers.is(2)));
        ContinuousSearchChipView chip =
                (ContinuousSearchChipView) layoutManager.findViewByPosition(2);
        Assert.assertNotNull(chip);
        Assert.assertTrue(chip.isTwoLineChipView());
        Assert.assertEquals("Default Result", chip.getPrimaryTextView().getText());

        // TODO(ckitagawa): access the ContinuousSearchSceneLayer class to validate its state. It
        // will have been setup in this test, but it is not readily accessible.

        // Close the UI by pretending another URL was opened and assert that the view is closed.
        View rootContainer =
                sActivityTestRule
                        .getActivity()
                        .findViewById(
                                org.chromium.chrome.browser.continuous_search.R.id.container_root);
        Assert.assertNotNull(rootContainer);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContinuousNavigationUserData.getForTab(tab)
                            .updateCurrentUrl(new GURL("https://other.com/"));
                });
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(rootContainer.getVisibility(), Matchers.is(View.GONE)));
    }
}
