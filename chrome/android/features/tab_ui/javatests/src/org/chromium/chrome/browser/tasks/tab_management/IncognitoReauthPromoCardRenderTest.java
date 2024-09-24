// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;

/** Render tests for incognito re-auth promo message card. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID})
@Batch(PER_CLASS)
public class IncognitoReauthPromoCardRenderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY_INCOGNITO)
                    .build();

    @Before
    public void setUp() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        IncognitoReauthPromoMessageService.setTriggerReviewActionWithoutReauthForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1376143")
    public void testRenderReauthPromoMessageCard_Portrait() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
        mRenderTestRule.render(
                cta.findViewById(R.id.large_message_card_item), "incognito_reauth_promo_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1376143")
    public void testRenderReauthPromoMessageCard_Landscape() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
        mRenderTestRule.render(
                cta.findViewById(R.id.large_message_card_item), "incognito_reauth_promo_landscape");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1376143")
    public void testRenderReauthPromoMessageCard_Snackbar() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_lock_action_text)).perform(click());
        onView(withId(R.id.snackbar)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_snackbar_text)).check(matches(isDisplayed()));
        mRenderTestRule.render(cta.findViewById(R.id.snackbar), "incognito_reauth_promo_snackbar");
    }
}
