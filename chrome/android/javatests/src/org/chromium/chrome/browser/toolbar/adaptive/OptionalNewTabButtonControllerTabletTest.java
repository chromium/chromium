// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Tests {@link OptionalNewTabButtonController} on tablet. Phone functionality is tested by {@link
 * OptionalNewTabButtonControllerPhoneTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:mode/always-new-tab"
})
@Restriction({DeviceFormFactor.TABLET})
public class OptionalNewTabButtonControllerTabletTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, /* clearAllTabState= */ false);

    private String mTestPageUrl;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(
                AdaptiveToolbarButtonVariant.NEW_TAB);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(null);
    }

    @Before
    public void setUp() {
        mTestPageUrl = sActivityTestRule.getTestServer().getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(sActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    public void testButton_hiddenOnTablet_landscape() {
        ActivityTestUtils.rotateActivityToOrientation(
                sActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        sActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);

        ViewUtils.waitForViewCheckingState(
                withId(R.id.optional_toolbar_button), ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL);
    }

    @Test
    @MediumTest
    public void testButton_hiddenOnTablet_portrait() {
        ActivityTestUtils.rotateActivityToOrientation(
                sActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        sActivityTestRule.loadUrl(mTestPageUrl, /* secondsToWait= */ 10);

        ViewUtils.waitForViewCheckingState(
                withId(R.id.optional_toolbar_button), ViewUtils.VIEW_GONE | ViewUtils.VIEW_NULL);
    }
}
