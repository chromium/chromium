// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.createMinimalCustomTabIntent;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for {@link CustomTabActivity} launched in ephemeral mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CCT_EPHEMERAL_MODE)
@Batch(Batch.PER_CLASS)
public class CustomTabActivityEphemeralTest {
    private static final String HISTOGRAM_NAME = "CustomTabs.IncognitoCCTCallerId";

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private String mTestPage;

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    private CustomTabsConnection mConnectionToCleanup;

    @Before
    public void setUp() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mAutomotiveRule.setIsAutomotive(false);
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mCustomTabActivityTestRule.getActivity() == null) return;
                    AppMenuCoordinator coordinator =
                            mCustomTabActivityTestRule.getAppMenuCoordinator();
                    // CCT doesn't always have a menu (ex. in the media viewer).
                    if (coordinator == null) return;
                    AppMenuHandler handler = coordinator.getAppMenuHandler();
                    if (handler != null) handler.hideAppMenu();
                });

        if (mConnectionToCleanup != null) {
            CustomTabsTestUtils.cleanupSessions(mConnectionToCleanup);
        }
    }

    private Intent createEphemeralCustomTabIntent() {
        return createMinimalCustomTabIntent(ApplicationProvider.getApplicationContext(), mTestPage)
                .putExtra(IntentHandler.EXTRA_ENABLE_EPHEMERAL_BROWSING, true);
    }

    private static int getThemeColor(CustomTabActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeColors.getDefaultThemeColor(activity, false));
    }

    private static int getToolbarColor(CustomTabActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CustomTabToolbar toolbar = activity.findViewById(R.id.toolbar);
                    return toolbar.getBackground().getColor();
                });
    }

    private void setCanUseHiddenTabForSession(
            CustomTabsConnection connection, CustomTabsSessionToken token, boolean useHiddenTab) {
        assert mConnectionToCleanup == null || mConnectionToCleanup == connection;
        // Save the connection. In case the hidden tab is not consumed by the test, ensure that it
        // is properly cleaned up after the test.
        mConnectionToCleanup = connection;
        connection.mClientManager.setHideDomainForSession(token, true);
        connection.setCanUseHiddenTabForSession(token, useHiddenTab);
    }

    private CustomTabActivity launchEphemeralCustomTabActivity() {
        Intent intent = createEphemeralCustomTabIntent();
        return launchCustomTabActivity(intent);
    }

    private CustomTabActivity launchCustomTabActivity(Intent intent) {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        return mCustomTabActivityTestRule.getActivity();
    }

    private void launchMenuItem(CustomTabActivity activity) {
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);
    }

    private void launchAndTestMenuItemIsNotVisible(
            CustomTabActivity activity, int itemId, String failureMessage) {
        launchMenuItem(activity);
        assertNull(
                failureMessage,
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), itemId));
    }

    private void launchAndTestMenuItemIsVisible(
            CustomTabActivity activity, int itemId, String failureMessage) {
        launchMenuItem(activity);
        assertNotNull(
                failureMessage,
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), itemId));
    }

    @Test
    @MediumTest
    public void testEphemeralTabLaunchesInOTRProfileWhenEnabled() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();
        Profile profile = activity.getActivityTab().getProfile();
        assertTrue(profile.isOffTheRecord());
        assertFalse(profile.isIncognitoBranded());
        assertFalse(profile.isPrimaryOTRProfile());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.CCT_EPHEMERAL_MODE)
    public void testEphemeralTabLaunchesInRegularProfileWhenDisabled() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();
        Profile profile = activity.getActivityTab().getProfile();
        assertFalse(profile.isOffTheRecord());
        assertFalse(profile.isIncognitoBranded());
        assertFalse(profile.isPrimaryOTRProfile());
    }

    @Test
    @MediumTest
    public void testToolbarDoesNotHaveIncognitoLogo() {
        launchEphemeralCustomTabActivity();
        onView(withId(R.id.incognito_cct_logo_image_view)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testToolbarHasOffTheRecordProfile() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();

        CustomTabToolbar customTabToolbar = activity.findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ToolbarDataProvider toolbarDataProvider =
                            customTabToolbar.getToolbarDataProvider();
                    assertTrue(toolbarDataProvider.isOffTheRecord());
                    assertFalse(toolbarDataProvider.isIncognitoBranded());
                });
    }

    @Test
    @MediumTest
    public void testToolbarHasDefaultThemeColor() {
        Intent intent = createEphemeralCustomTabIntent();
        CustomTabActivity activity = launchCustomTabActivity(intent);
        assertEquals(getThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @MediumTest
    public void testCanCustomizeToolbarColor() {
        Intent intent = createEphemeralCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED);
        CustomTabActivity activity = launchCustomTabActivity(intent);
        assertEquals(Color.RED, getToolbarColor(activity));
    }

    @Test
    @MediumTest
    public void testCloseAllIncognitoNotificationIsNotDisplayed() {
        // It may happen that some previous incognito notification from tabbed activity may be
        // already be lying around. So, we test the delta instead to be 0.
        Context context = ContextUtils.getApplicationContext();
        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        int startCount = notificationManager.getActiveNotifications().length;

        // Launch Ephemeral CCT
        launchEphemeralCustomTabActivity();

        int endCount = notificationManager.getActiveNotifications().length;
        assertEquals(0, endCount - startCount);
    }

    @Test
    @MediumTest
    public void testHiddenTabCreationIsBlocked() throws Exception {
        // mayLaunchUrl should be blocked for ephemeral mode since it runs with always regular
        // profile. Need to update the test if the mayLaunchUrl is ever
        // allowed in OTR profiles. (crbug.com/1106757)
        Intent intent = createEphemeralCustomTabIntent();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        // Passes the launch intent to the connection.
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse(mTestPage), intent.getExtras(), null));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Hidden tab was created",
                            connection.getHiddenTabForTesting(),
                            Matchers.nullValue());
                });
        launchCustomTabActivity(intent);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
    public void testHistoryMenuItemIsHidden() {
        Intent intent = createEphemeralCustomTabIntent();
        intent.putExtra(IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE, "com.foo.bar");
        CustomTabActivity activity = launchCustomTabActivity(intent);

        launchAndTestMenuItemIsNotVisible(
                activity, R.id.open_history_menu_id, "History item is visible");
    }

    @Test
    @MediumTest
    public void testDownloadTopIconIsHidden() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();

        launchAndTestMenuItemIsNotVisible(
                activity, R.id.offline_page_id, "Download icon is visible");
    }

    @Test
    @MediumTest
    public void testOpenInChromeIncognitoMenuItemIsVisible() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();

        launchAndTestMenuItemIsVisible(
                activity, R.id.open_in_browser_id, "Open in browser not visible");
        onView(withText(R.string.menu_open_in_incognito_chrome)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
    public void testIncognitoReauthPageNotShown() throws Exception {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        Intent intent = createEphemeralCustomTabIntent();
        CustomTabActivity customTabActivity = launchCustomTabActivity(intent);
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OneshotSupplier<IncognitoReauthController>
                            incognitoReauthControllerOneshotSupplier =
                                    customTabActivity
                                            .getRootUiCoordinatorForTesting()
                                            .getIncognitoReauthControllerSupplier();
                    CallbackController callbackController = new CallbackController();
                    incognitoReauthControllerOneshotSupplier.onAvailable(
                            callbackController.makeCancelable(
                                    incognitoReauthController -> {
                                        assertNotNull(incognitoReauthController);
                                        callbackHelper.notifyCalled();
                                    }));
                });
        callbackHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
                    IncognitoReauthController incognitoReauthController =
                            customTabActivity
                                    .getRootUiCoordinatorForTesting()
                                    .getIncognitoReauthControllerSupplier()
                                    .get();

                    // Fake Chrome going background and coming back to foreground.
                    ApplicationStatus.TaskVisibilityListener visibilityListener =
                            (ApplicationStatus.TaskVisibilityListener) incognitoReauthController;
                    visibilityListener.onTaskVisibilityChanged(
                            customTabActivity.getTaskId(), false);

                    StartStopWithNativeObserver observer =
                            (StartStopWithNativeObserver) incognitoReauthController;
                    observer.onStartWithNative();

                    assertFalse(
                            "Re-auth screen should not be shown.",
                            incognitoReauthController.isReauthPageShowing());

                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false);
                });

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.SEARCH_IN_CCT)
    public void testNonInteractiveOmnibox() {
        CustomTabActivity activity = launchEphemeralCustomTabActivity();
        var tab = activity.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage);

        var titleBar = activity.findViewById(R.id.title_url_container);
        Assert.assertFalse(titleBar.hasOnClickListeners());
    }

    @Test
    @MediumTest
    public void recordsHistogramEphemeral() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NAME, IntentHandler.IncognitoCCTCallerId.EPHEMERAL_TAB);
        launchEphemeralCustomTabActivity();
        histogramWatcher.assertExpected();
    }
}
