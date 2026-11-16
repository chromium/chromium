// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.LargeTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthControllerImpl;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Tests the incognito re-auth state of a {@link ChromeTabbedActivity} that is recreated while it is
 * in the foreground.
 *
 * <p>The activity is recreated through {@link ChromeTabbedActivity#onNightModeStateChanged}, which
 * takes the same path as a density change after a move to another display.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests recreate the activity and change the Incognito lock setting.")
public class IncognitoReauthRecreationTest {
    @Rule
    public final FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
    }

    @After
    public void tearDown() {
        setIncognitoLockEnabled(false);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
    }

    /** The user did not leave Chrome, so a recreation alone must not ask for re-auth. */
    @Test
    @LargeTest
    @Feature({"Incognito"})
    public void testForegroundRecreation_doesNotRequireReauth() {
        ChromeTabbedActivity activity = openIncognitoPage();
        setIncognitoLockEnabled(true);

        ChromeTabbedActivity recreated = recreate(activity);

        assertFalse(
                "Re-auth should not be pending after a foreground recreation",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getReauthController(recreated).isIncognitoReauthPending()));
        assertFalse(
                "Re-auth page should not be shown after a foreground recreation",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getReauthController(recreated).isReauthPageShowing()));
    }

    /** A recreation must not dismiss a re-auth that was already required. */
    @Test
    @LargeTest
    @Feature({"Incognito"})
    public void testForegroundRecreation_keepsPendingReauth() {
        ChromeTabbedActivity activity = openIncognitoPage();
        setIncognitoLockEnabled(true);

        // Make re-auth pending as if the task had been in the background and came back.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoReauthControllerImpl controller =
                            (IncognitoReauthControllerImpl) getReauthController(activity);
                    controller.onTaskVisibilityChanged(activity.getTaskId(), false);
                    controller.onStartWithNative();
                    assertTrue(
                            "Re-auth page should be shown before the recreation",
                            controller.isReauthPageShowing());
                });

        ChromeTabbedActivity recreated = recreate(activity);

        assertTrue(
                "Re-auth should still be pending after the recreation",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getReauthController(recreated).isIncognitoReauthPending()));
        CriteriaHelper.pollUiThread(
                () -> getReauthController(recreated).isReauthPageShowing(),
                "Re-auth page should be shown again after the recreation");
    }

    /**
     * Opens an incognito tab on a web page and returns its activity.
     *
     * <p>A recreated activity restores incognito tabs only if the state of an incognito tab was
     * saved before. The state of an incognito New Tab Page is not saved, so the tab must show a web
     * page.
     */
    private ChromeTabbedActivity openIncognitoPage() {
        WebPageStation incognitoPage =
                mCtaTestRule.startOnBlankPage().openNewIncognitoTabOrWindowFast().loadAboutBlank();
        ChromeTabbedActivity activity = incognitoPage.getActivity();
        CriteriaHelper.pollUiThread(
                () -> TabStateExtractor.from(activity.getActivityTab()).contentsState != null,
                "The incognito tab should have a state to save");
        return activity;
    }

    private static void setIncognitoLockEnabled(boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, enabled));
    }

    /**
     * Recreates {@code activity} and returns the new activity once its tab state is initialized,
     * which is when the saved re-auth state has been applied.
     */
    private static ChromeTabbedActivity recreate(ChromeTabbedActivity activity) {
        ChromeTabbedActivity recreated =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        activity::onNightModeStateChanged);
        CriteriaHelper.pollUiThread(
                () ->
                        recreated.didFinishNativeInitialization()
                                && recreated.getTabModelSelector().isTabStateInitialized()
                                && getReauthController(recreated) != null,
                "The recreated activity should restore its tabs");
        // Without an incognito tab, re-auth is never pending.
        assertTrue(
                "The incognito tab should be restored",
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                recreated
                                                .getTabModelSelector()
                                                .getModel(/* incognito= */ true)
                                                .getCount()
                                        > 0));
        return recreated;
    }

    private static IncognitoReauthController getReauthController(ChromeTabbedActivity activity) {
        return activity.getRootUiCoordinatorForTesting()
                .getIncognitoReauthControllerSupplier()
                .get();
    }
}
