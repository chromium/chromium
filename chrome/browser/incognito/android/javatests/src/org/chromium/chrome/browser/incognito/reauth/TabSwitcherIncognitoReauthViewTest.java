// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.IsNot.not;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.addBlankTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;

/** Tests for Incognito reauth view layout in Tab Switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID,
    ChromeFeatureList.INCOGNITO_SCREENSHOT
})
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
@Batch(Batch.PER_CLASS)
public class TabSwitcherIncognitoReauthViewTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY_INCOGNITO)
                    .setRevision(3)
                    .build();

    @Before
    public void setUp() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false);
                });
    }

    private void triggerIncognitoReauthCustomView(ChromeTabbedActivity cta) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoReauthController incognitoReauthController =
                            cta.getRootUiCoordinatorForTesting()
                                    .getIncognitoReauthControllerSupplier()
                                    .get();

                    // Fake Chrome going background and coming back to foreground.
                    ApplicationStatus.TaskVisibilityListener visibilityListener =
                            (ApplicationStatus.TaskVisibilityListener) incognitoReauthController;
                    visibilityListener.onTaskVisibilityChanged(cta.getTaskId(), false);

                    StartStopWithNativeObserver observer =
                            (StartStopWithNativeObserver) incognitoReauthController;
                    observer.onStartWithNative();

                    assertTrue(
                            "Re-auth screen should be shown.",
                            incognitoReauthController.isReauthPageShowing());
                });
    }

    private void openIncognitoReauth(ChromeTabbedActivity cta) {
        // Open incognito tab.
        addBlankTabs(cta, true, 1);

        assertTrue(cta.getActivityTab().isIncognito());

        // Enter tab switcher in incognito mode.
        enterTabSwitcher(cta);
        assertTrue(cta.getTabModelSelector().isIncognitoSelected());

        // Reload chrome to trigger incognito reauth screen.
        triggerIncognitoReauthCustomView(cta);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testIncognitoReauthView_HubRenderTest() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        openIncognitoReauth(cta);

        onView(withId(R.id.hub_toolbar)).check(matches(isDisplayed()));
        onView(withId(R.id.toolbar_action_button)).check(matches(not(isEnabled())));
        onView(withId(R.id.incognito_reauth_menu_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_unlock_incognito_button_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_see_other_tabs_label))
                .check(matches(not(isDisplayed())));
        onView(withText(R.string.incognito_reauth_page_see_other_tabs_label))
                .check(matches(not(isDisplayed())));

        mRenderTestRule.render(
                cta.findViewById(org.chromium.chrome.R.id.tab_switcher_view_holder),
                "incognito_reauth_view_hub");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON})
    public void testIncognitoReauthView_HubRenderTest_FloatingActionButton() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        openIncognitoReauth(cta);

        onView(withId(R.id.hub_toolbar)).check(matches(isDisplayed()));
        onView(withId(R.id.host_action_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.incognito_reauth_menu_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_unlock_incognito_button_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_see_other_tabs_label))
                .check(matches(not(isDisplayed())));
        onView(withText(R.string.incognito_reauth_page_see_other_tabs_label))
                .check(matches(not(isDisplayed())));

        mRenderTestRule.render(
                cta.findViewById(org.chromium.chrome.R.id.tab_switcher_view_holder),
                "incognito_reauth_view_hub_floating_action_button");
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/330226530")
    public void testIncognitoReauthViewIsRestored_WhenActivityIsKilled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        openIncognitoReauth(cta);

        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                () -> {
                    return TabStateExtractor.from(cta.getActivityTab()).contentsState != null;
                });

        mActivityTestRule.recreateActivity();

        onViewWaiting(
                        withId(R.id.incognito_reauth_unlock_incognito_button),
                        true) // checkRootDialog=true ensures dialog is in focus, avoids flakiness.
                .check(matches(isDisplayed()));
    }
}
