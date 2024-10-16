// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;
import static org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.addActionButtonToIntent;
import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.createTestBitmap;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RemoteViews;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CallbackController;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.OnFinishedForTest;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link CustomTabActivity} launched in incognito mode.
 * TODO(crbug.com/2338935): Add the screenshot rule again once there's a reliable way to take them
 * in the first place. Screenshot of the Custom tab menu item is broken.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class CustomTabActivityIncognitoTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_MENU_TITLE = "testMenuTitle";
    private static int sIdToIncrement = 1;
    private String mTestPage;

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule public JniMocker jniMocker = new JniMocker();
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;

    @Before
    public void setUp() throws TimeoutException {
        MockitoAnnotations.initMocks(this);

        // Mock translate bridge so "Translate..." menu item doesn't unexpectedly show up.
        jniMocker.mock(
                org.chromium.chrome.browser.translate.TranslateBridgeJni.TEST_HOOKS,
                mTranslateBridgeJniMock);
        jniMocker.mock(TranslateBridgeJni.TEST_HOOKS, mTranslateBridgeJniMock);

        FirstRunStatus.setFirstRunFlowComplete(true);
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
    }

    private Bitmap createVectorDrawableBitmap(@DrawableRes int resId, int widthDp, int heightDp) {
        Context context = ApplicationProvider.getApplicationContext();
        Drawable vectorDrawable = AppCompatResources.getDrawable(context, resId);
        Bitmap bitmap = createTestBitmap(widthDp, heightDp);
        Canvas canvas = new Canvas(bitmap);
        float density = context.getResources().getDisplayMetrics().density;
        int widthPx = (int) (density * widthDp);
        int heightPx = (int) (density * heightDp);
        vectorDrawable.setBounds(0, 0, widthPx, heightPx);
        vectorDrawable.draw(canvas);
        return bitmap;
    }

    private Intent createTestCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    private static int getIncognitoThemeColor(CustomTabActivity activity) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeColors.getDefaultThemeColor(activity, true));
    }

    private static int getToolbarColor(CustomTabActivity activity) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CustomTabToolbar toolbar = activity.findViewById(R.id.toolbar);
                    return toolbar.getBackground().getColor();
                });
    }

    private void launchMenuItem() throws Exception {
        Intent intent = createTestCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);
    }

    private void launchAndTestMenuItemIsVisible(int itemId, String screenshotName)
            throws Exception {
        launchMenuItem();
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), itemId));
    }

    private void launchAndTestMenuItemIsNotVisible(int itemId, String screenshotName)
            throws Exception {
        launchMenuItem();
        assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), itemId));
    }

    private void testTopActionIconsIsVisible() throws Exception {
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.forward_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.reload_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.bookmark_this_page_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.info_menu_id));

        ModelList iconRowModelList =
                AppMenuTestSupport.getMenuItemPropertyModel(
                                mCustomTabActivityTestRule.getAppMenuCoordinator(),
                                R.id.icon_row_menu_id)
                        .get(AppMenuItemProperties.SUBMENU);

        int expectedTopActionIconsCount = 4;
        assertEquals(expectedTopActionIconsCount, iconRowModelList.size());
    }

    private CustomTabActivity launchIncognitoCustomTab(Intent intent) throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        return mCustomTabActivityTestRule.getActivity();
    }

    private void assertProfileUsedIsNonPrimary() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            Profile.fromWebContents(
                                    mCustomTabActivityTestRule
                                            .getActivity()
                                            .getCurrentWebContents());
                    assertTrue(profile.isOffTheRecord());
                    assertFalse(profile.isPrimaryOTRProfile());
                    assertTrue(profile.isIncognitoBranded());
                });
    }

    @Test
    @MediumTest
    public void launchesInOffTheRecordWhenEnabled() throws Exception {
        Intent intent = createTestCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertTrue(activity.getActivityTab().isIncognito());
        assertProfileUsedIsNonPrimary();
    }

    @Test
    @MediumTest
    public void toolbarHasIncognitoThemeColor() throws Exception {
        Intent intent = createTestCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @MediumTest
    public void toolbarHasIncognitoLogo() throws Exception {
        Intent intent = createTestCustomTabIntent();
        launchIncognitoCustomTab(intent);

        onView(withId(R.id.incognito_cct_logo_image_view)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void toolbarHasNonPrimaryOffTheRecordProfile() throws Exception {
        Intent intent = createTestCustomTabIntent();
        launchIncognitoCustomTab(intent);

        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
                    assertTrue(profile.isOffTheRecord());
                    assertFalse(profile.isPrimaryOTRProfile());
                    assertTrue(profile.isIncognitoBranded());
                });
    }

    @Test
    @MediumTest
    public void toolbarHasRegularProfile_ForRegularCCT() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
                    assertFalse(profile.isOffTheRecord());
                    assertFalse(profile.isIncognitoBranded());
                });
    }

    @Test
    @MediumTest
    public void ignoresCustomizedToolbarColor() throws Exception {
        Intent intent = createTestCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);

        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @MediumTest
    public void closeAllIncognitoNotificationIsNotDisplayed() throws Exception {
        // It may happen that some previous incognito notification from tabbed activity may be
        // already be lying around. So, we test the delta instead to be 0.
        Context context = ContextUtils.getApplicationContext();
        NotificationManager nm =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        int startCount = nm.getActiveNotifications().length;

        // Launch incognito CCT
        Intent intent = createTestCustomTabIntent();
        launchIncognitoCustomTab(intent);

        int endCount = nm.getActiveNotifications().length;
        assertEquals(0, endCount - startCount);
    }

    @Test
    @MediumTest
    public void openInBrowserMenuItemIsNotVisible() throws Exception {
        launchAndTestMenuItemIsNotVisible(R.id.open_in_browser_id, "Open in Browser not visible");
    }

    @Test
    @MediumTest
    public void doesNotHaveAddToHomeScreenMenuItem() throws Exception {
        launchAndTestMenuItemIsNotVisible(R.id.universal_install, "Install not visible");
    }

    @Test
    @MediumTest
    public void bookmarkTopIconIsVisible() throws Exception {
        launchAndTestMenuItemIsVisible(R.id.bookmark_this_page_id, "Bookmark icon is visible");
    }

    @Test
    @MediumTest
    public void downloadTopIconIsNotVisible() throws Exception {
        launchAndTestMenuItemIsNotVisible(R.id.offline_page_id, "Download icon not visible");
    }

    @Test
    @MediumTest
    public void shareMenuItemByDefaultIsNotVisibile() throws Exception {
        launchAndTestMenuItemIsNotVisible(
                R.id.share_row_menu_id, "Share menu item not visible by default");
    }

    @Test
    @MediumTest
    public void shareMenuItemViaIntentExtraIsVisibile() throws Exception {
        Intent intent = createTestCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.share_row_menu_id));
    }

    @Test
    @MediumTest
    public void ensureOnlyFourTopIconsAreVisible() throws Exception {
        launchMenuItem();
        testTopActionIconsIsVisible();
    }

    @Test
    @MediumTest
    public void ensureAddCustomMenuItemHasNoEffect() throws Exception {
        Intent intent = createTestCustomTabIntent();
        CustomTabsIntentTestUtils.addMenuEntriesToIntent(intent, 3, TEST_MENU_TITLE);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());

        // Check the menu items have only 3 items visible including the top icon row menu for
        // incognito tabs.
        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, 3);

        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.icon_row_menu_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.request_desktop_site_row_menu_id));

        // Check top icons are still the same.
        testTopActionIconsIsVisible();
    }

    @Test
    @MediumTest
    public void ensureAddCustomMenuItemIsEnabledForReaderMode() throws Exception {
        Intent intent = createTestCustomTabIntent();
        CustomTabIntentDataProvider.addReaderModeUIExtras(intent);
        IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                intent, IntentHandler.IncognitoCCTCallerId.READER_MODE);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        ModelList menuItemsModelList =
                AppMenuTestSupport.getMenuModelList(
                        mCustomTabActivityTestRule.getAppMenuCoordinator());
        // Check the menu items have only 2 items visible "not" including the top icon row menu.
        CustomTabsTestUtils.assertMenuSize(menuItemsModelList, 2);
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.reader_mode_prefs_id));
        assertNotNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.find_in_page_id));

        assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(), R.id.icon_row_menu_id));
        assertNull(
                AppMenuTestSupport.getMenuItemPropertyModel(
                        mCustomTabActivityTestRule.getAppMenuCoordinator(),
                        R.id.request_desktop_site_row_menu_id));
    }

    @Test
    @MediumTest
    public void ensureAddCustomTopMenuItemHasNoEffect() throws Exception {
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        Intent intent = createTestCustomTabIntent();
        final PendingIntent pi =
                addActionButtonToIntent(intent, expectedIcon, "Good test", sIdToIncrement++);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        activity.getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        Assert.assertNull(actionButton);
    }

    @Test
    @MediumTest
    public void ensureAddRemoteViewsHasNoEffect() throws Exception {
        Intent intent = createTestCustomTabIntent();
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        final PendingIntent pi =
                addActionButtonToIntent(intent, expectedIcon, "Good test", sIdToIncrement++);

        // Create a RemoteViews. The layout used here is pretty much arbitrary, but with the
        // constraint that a) it already exists in production code, and b) it only contains
        // views with the @RemoteView annotation.
        RemoteViews remoteViews =
                new RemoteViews(
                        ApplicationProvider.getApplicationContext().getPackageName(),
                        R.layout.share_sheet_item);
        remoteViews.setTextViewText(R.id.text, "Kittens!");
        remoteViews.setTextViewText(R.id.display_new, "So fluffy");
        remoteViews.setImageViewResource(R.id.icon, R.drawable.ic_email_googblue_36dp);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS, remoteViews);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS, new int[] {R.id.icon});

        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        activity.getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(onFinished);

        View bottomBarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.bottom_bar);
        assertTrue(bottomBarView == null);
    }

    @Test
    @MediumTest
    public void ensureMayLaunchUrlIsBlockedForIncognitoWithExtraInConnection() throws Exception {
        // mayLaunchUrl should be blocked for incognito mode since it runs with always regular
        // profile. Need to update the test if the mayLaunchUrl is ever
        // allowed in incognito. (crbug.com/1106757)
        Intent intent = createTestCustomTabIntent();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        // Passes the launch intent to the connection.
        mCustomTabActivityTestRule.buildSessionWithHiddenTab(connection, token);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse(mTestPage), intent.getExtras(), null));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab was created",
                            connection.getSpeculationParamsForTesting(),
                            Matchers.nullValue());
                },
                LONG_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        mCustomTabActivityTestRule.setCustomSessionInitiatedForIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @MediumTest
    public void ensureHiddenTabIsBlockedForIncognitoWithoutExtraInConnection() throws Exception {
        // Creation of hidden tab should be blocked for incognito mode for the same setup as regular
        // mode above. Currently hidden tabs are created always with regular profile, so we
        // should block the hidden tab creation. Need to update the test if the hidden tabs are
        // allowed in incognito. (crbug.com/1190971)
        Intent intent = createTestCustomTabIntent();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        // Passes null intent here to mimic not having incognito extra in intent at the connection.
        mCustomTabActivityTestRule.buildSessionWithHiddenTab(connection, token);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab was not created",
                            connection.getSpeculationParamsForTesting(),
                            Matchers.notNullValue());
                },
                LONG_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeTabUtils.waitForTabPageLoaded(
                connection.getSpeculationParamsForTesting().tab, mTestPage);
        mCustomTabActivityTestRule.setCustomSessionInitiatedForIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        connection.cleanUpSession(token);
    }

    /** Regression test for crbug.com/1325331. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
    public void testIncognitoReauthControllerCreated_WhenReauthFeatureIsEnabled()
            throws InterruptedException, TimeoutException {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        Intent intent = createTestCustomTabIntent();
        CustomTabActivity customTabActivity = launchIncognitoCustomTab(intent);
        CallbackHelper callbackHelper = new CallbackHelper();
        // Ensure that we did indeed create the re-auth controller.
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
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
    public void testIncognitoReauthPageShowing() throws Exception {
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);

        Intent intent = createTestCustomTabIntent();
        CustomTabActivity customTabActivity = launchIncognitoCustomTab(intent);
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

                    assertTrue(
                            "Re-auth screen should be shown.",
                            incognitoReauthController.isReauthPageShowing());

                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false);
                });

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
    }
}
