// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;
import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.addActionButtonToIntent;
import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.createTestBitmap;
import static org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider.EXTRA_FORCE_ENABLE_FOR_EXPERIMENT;

import android.annotation.TargetApi;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RemoteViews;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.OnFinishedForTest;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link CustomTabActivity} launched in incognito mode.
 * TODO(crbug.com/2338935): Add the screenshot rule again once there's a reliable way to take them
 * in the first place. Screenshot of the Custom tab menu item is broken.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_FIRST_RUN_FLOW_COMPLETE_FOR_TESTING})
public class CustomTabActivityIncognitoTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_MENU_TITLE = "testMenuTitle";
    private static int sIdToIncrement = 1;

    private String mTestPage;

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Before
    public void setUp() throws TimeoutException {
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        // Ensuring native is initialized before we access the CCT_INCOGNITO feature flag.
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
    }

    private Bitmap createVectorDrawableBitmap(@DrawableRes int resId, int widthDp, int heightDp) {
        Context context = InstrumentationRegistry.getTargetContext();
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

    private Intent createMinimalIncognitoCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalIncognitoCustomTabIntent(
                InstrumentationRegistry.getContext(), mTestPage);
    }

    private static int getIncognitoThemeColor(CustomTabActivity activity) throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeColors.getDefaultThemeColor(activity.getResources(), true));
    }

    private static int getToolbarColor(CustomTabActivity activity) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            CustomTabToolbar toolbar = activity.findViewById(R.id.toolbar);
            return toolbar.getBackground().getColor();
        });
    }

    private void launchMenuItem() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);
    }

    private void launchAndTestMenuItemIsVisible(int itemId, String screenshotName)
            throws Exception {
        launchMenuItem();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        MenuItem item = menu.findItem(itemId);
        assertTrue(item.isVisible());
    }

    private void launchAndTestMenuItemIsNotVisible(int itemId, String screenshotName)
            throws Exception {
        launchMenuItem();
        Menu menu = mCustomTabActivityTestRule.getMenu();
        MenuItem item = menu.findItem(itemId);
        assertTrue(item == null || !item.isVisible());
    }

    private void testTopActionIconsIsVisible() throws Exception {
        Menu menu = mCustomTabActivityTestRule.getMenu();
        MenuItem iconRow = menu.findItem(R.id.icon_row_menu_id);

        assertEquals(4, CustomTabsTestUtils.getVisibleMenuSize(iconRow.getSubMenu()));
        assertTrue(iconRow.getSubMenu().findItem(R.id.forward_menu_id).isVisible());
        assertTrue(iconRow.getSubMenu().findItem(R.id.reload_menu_id).isVisible());
        assertTrue(iconRow.getSubMenu().findItem(R.id.bookmark_this_page_id).isVisible());
        assertTrue(iconRow.getSubMenu().findItem(R.id.info_menu_id).isVisible());
    }

    private CustomTabActivity launchIncognitoCustomTab(Intent intent) throws InterruptedException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        return mCustomTabActivityTestRule.getActivity();
    }

    private void assertProfileUsedIsNonPrimary() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.fromWebContents(
                    mCustomTabActivityTestRule.getActivity().getCurrentWebContents());
            assertTrue(profile.isOffTheRecord());
            assertFalse(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void launchesIncognitoWhenEnabled() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertTrue(activity.getActivityTab().isIncognito());
        assertProfileUsedIsNonPrimary();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void doesntLaunchIncognitoWhenDisabled() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertFalse(activity.getActivityTab().isIncognito());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void toolbarHasIncognitoThemeColor() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void toolbarHasIncognitoLogo() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        launchIncognitoCustomTab(intent);
        Espresso.onView(withId(R.id.incognito_cct_logo_image_view)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void toolbarDoesNotHaveIncognitoLogo() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        launchIncognitoCustomTab(intent);
        Espresso.onView(withId(R.id.incognito_cct_logo_image_view))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void toolbarHasNonPrimaryIncognitoProfile_ForIncognitoCCT() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        launchIncognitoCustomTab(intent);

        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
            assertTrue(profile.isOffTheRecord());
            assertFalse(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    public void toolbarHasRegularProfile_ForRegularCCT() {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
            assertFalse(profile.isOffTheRecord());
        });
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void canLaunchFirstPartyIncognitoWithExtraWhenDisabled() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        intent.putExtra(EXTRA_FORCE_ENABLE_FOR_EXPERIMENT, true);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertTrue(activity.getActivityTab().isIncognito());
        assertProfileUsedIsNonPrimary();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void canHideToolbarIncognitoLogo() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        // The icon is only hidden if an extra is supplied.
        intent.putExtra(IncognitoCustomTabIntentDataProvider.EXTRA_HIDE_INCOGNITO_ICON, true);
        launchIncognitoCustomTab(intent);
        Espresso.onView(withId(R.id.incognito_cct_logo_image_view))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void canCustomizeToolbarColor() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        // The color is only allowed if an extra is supplied.
        intent.putExtra(IncognitoCustomTabIntentDataProvider.EXTRA_USE_NORMAL_PROFILE_STYLE, true);
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertEquals(Color.RED, getToolbarColor(activity));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ignoresCustomizedToolbarColor() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, Color.RED);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        assertEquals(getIncognitoThemeColor(activity), getToolbarColor(activity));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    @TargetApi(Build.VERSION_CODES.M)
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void closeAllIncognitoNotificationIsNotDisplayed() throws Exception {
        // It may happen that some previous incognito notification from tabbed activity may be
        // already be lying around. So, we test the delta instead to be 0.
        Context context = ContextUtils.getApplicationContext();
        NotificationManager nm =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        int startCount = nm.getActiveNotifications().length;

        // Launch incognito CCT
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabActivity activity = launchIncognitoCustomTab(intent);

        int endCount = nm.getActiveNotifications().length;
        assertEquals(0, endCount - startCount);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void openInBrowserMenuItemIsNotVisible() throws Exception {
        launchAndTestMenuItemIsNotVisible(R.id.open_in_browser_id, "Open in Browser not visible");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void doesNotHaveAddToHomeScreenMenuItem() throws Exception {
        launchAndTestMenuItemIsNotVisible(
                R.id.add_to_homescreen_id, "Add to home screen not visible");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void bookmarkTopIconIsVisible() throws Exception {
        launchAndTestMenuItemIsVisible(R.id.bookmark_this_page_id, "Bookmark icon is visible");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void downloadTopIconIsNotVisible() throws Exception {
        launchAndTestMenuItemIsNotVisible(R.id.offline_page_id, "Download icon not visible");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void shareMenuItemByDefaultIsNotVisibile() throws Exception {
        launchAndTestMenuItemIsNotVisible(
                R.id.share_row_menu_id, "Share menu item not visible by default");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void shareMenuItemViaIntentExtraIsVisibile() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        intent.putExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, true);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        MenuItem item = mCustomTabActivityTestRule.getMenu().findItem(R.id.share_row_menu_id);
        assertTrue(item.isVisible());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureOnlyFourTopIconsAreVisible() throws Exception {
        launchMenuItem();
        testTopActionIconsIsVisible();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureAddCustomMenuItemHasNoEffect() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabsTestUtils.addMenuEntriesToIntent(intent, 3, TEST_MENU_TITLE);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        Menu menu = mCustomTabActivityTestRule.getMenu();
        // Check the menu items have only 3 items visible including the top icon row menu.
        assertEquals(3, CustomTabsTestUtils.getVisibleMenuSize(menu));
        assertTrue(menu.findItem(R.id.icon_row_menu_id).isVisible());
        assertTrue(menu.findItem(R.id.find_in_page_id).isVisible());
        assertTrue(menu.findItem(R.id.request_desktop_site_row_menu_id).isVisible());

        // Check top icons are still the same.
        testTopActionIconsIsVisible();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureAddCustomMenuItemIsEnabledForReaderMode() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabIntentDataProvider.addReaderModeUIExtras(intent);
        IncognitoCustomTabIntentDataProvider.addIncongitoExtrasForChromeFeatures(
                intent, IntentHandler.IncognitoCCTCallerId.READER_MODE);
        CustomTabActivity activity = launchIncognitoCustomTab(intent);
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(activity);

        Menu menu = mCustomTabActivityTestRule.getMenu();
        // Check the menu items have only 2 items visible "not" including the top icon row menu.
        assertEquals(2, CustomTabsTestUtils.getVisibleMenuSize(menu));
        assertTrue(menu.findItem(R.id.reader_mode_prefs_id).isVisible());
        assertTrue(menu.findItem(R.id.find_in_page_id).isVisible());

        assertFalse(menu.findItem(R.id.icon_row_menu_id).isVisible());
        assertFalse(menu.findItem(R.id.request_desktop_site_row_menu_id).isVisible());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureAddCustomTopMenuItemHasNoEffect() throws Exception {
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        Intent intent = createMinimalIncognitoCustomTabIntent();
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureAddRemoteViewsHasNoEffect() throws Exception {
        Intent intent = createMinimalIncognitoCustomTabIntent();
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        final PendingIntent pi =
                addActionButtonToIntent(intent, expectedIcon, "Good test", sIdToIncrement++);

        // Create a RemoteViews. The layout used here is pretty much arbitrary, but with the
        // constraint that a) it already exists in production code, and b) it only contains
        // views with the @RemoteView annotation.
        RemoteViews remoteViews =
                new RemoteViews(InstrumentationRegistry.getTargetContext().getPackageName(),
                        R.layout.web_notification);
        remoteViews.setTextViewText(R.id.title, "Kittens!");
        remoteViews.setTextViewText(R.id.body, "So fluffy");
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
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureMayLaunchUrlIsBlockedForIncognitoWithExtraInConnection() throws Exception {
        // mayLaunchUrl should be blocked for incognito mode since it runs with always regular
        // profile. Need to update the test if the mayLaunchUrl is ever
        // allowed in incognito. (crbug.com/1106757)
        Intent intent = createMinimalIncognitoCustomTabIntent();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        // Passes the launch intent to the connection.
        mCustomTabActivityTestRule.buildSessionWithHiddenTab(connection, token);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse(mTestPage), intent.getExtras(), null));
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Tab was created", connection.getSpeculationParamsForTesting(),
                    Matchers.nullValue());
        }, LONG_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        mCustomTabActivityTestRule.setCustomSessionInitiatedForIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void ensureHiddenTabIsBlockedForIncognitoWithoutExtraInConnection() throws Exception {
        // Creation of hidden tab should be blocked for incognito mode for the same setup as regular
        // mode above. Currently hidden tabs are created always with regular profile, so we
        // should block the hidden tab creation. Need to update the test if the hidden tabs are
        // allowed in incognito. (crbug.com/1190971)
        Intent intent = createMinimalIncognitoCustomTabIntent();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        // Passes null intent here to mimic not having incognito extra in intent at the connection.
        mCustomTabActivityTestRule.buildSessionWithHiddenTab(connection, token);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Tab was not created", connection.getSpeculationParamsForTesting(),
                    Matchers.notNullValue());
        }, LONG_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeTabUtils.waitForTabPageLoaded(
                connection.getSpeculationParamsForTesting().tab, mTestPage);
        mCustomTabActivityTestRule.setCustomSessionInitiatedForIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        connection.cleanUpSession(token);
    }
}
