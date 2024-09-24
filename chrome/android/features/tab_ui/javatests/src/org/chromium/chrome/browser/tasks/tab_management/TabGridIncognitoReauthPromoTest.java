// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Instrumentation tests for the incognito re-auth promo component.
 *
 * <p>TODO(crbug.com/40056462): Remove the restriction on only phone type and make it available for
 * tablets. Also, remove the restriction on running this suite only for high end phones when GTS is
 * available for them.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID})
@DisableFeatures({ChromeFeatureList.ANDROID_HUB_SEARCH})
@DoNotBatch(reason = "Batching can cause message state to leak between tests.")
public class TabGridIncognitoReauthPromoTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        IncognitoReauthPromoMessageService.setTriggerReviewActionWithoutReauthForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();

        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                TabSwitcherMessageManager::resetHasAppendedMessagesForTesting);
    }

    @Test
    @MediumTest
    public void testIncognitoReauthPromoShown() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);

        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSnackBarShown_WhenClickingReviewActionProvider() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);

        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        onView(withText(R.string.incognito_reauth_lock_action_text)).perform(click());
        onView(withId(R.id.snackbar)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_snackbar_text)).check(matches(isDisplayed()));

        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testIncognitoPromoNotShownInRegularMode() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, false, 1);
        enterTabSwitcher(cta);

        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testIncognitoPromoNotShownInRegularMode_WhenTogglingFromIncognito() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, false, 1);
        createTabs(cta, true, 1);
        enterTabSwitcher(cta);

        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        switchTabModel(cta, false);
        assertFalse(cta.getTabModelSelector().getCurrentModel().isIncognito());
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testIncognitoReauthPromo_NoThanks_HidesTheCard() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);

        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
        onView(withId(R.id.secondary_action_button)).perform(click());

        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }
}
