// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_HEIGHT_PX;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_INITIAL_ACTIVITY_WIDTH_PX;
import static androidx.browser.customtabs.CustomTabsIntent.SHARE_STATE_ON;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.createTestBitmap;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.SystemClock;
import android.provider.Browser;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.ContentFrameLayout;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.PrefetchOptions;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.TabsOpenedFromExternalAppTest;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker.ClientIdentifierType;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils.OnFinishedForTest;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.PrefetchTestUtil;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;

/** Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "Some tests are Testing CCT start up behavior. "
                        + "Unit test conversion tracked in crbug.com/1217031")
public class CustomTabActivityTest {
    private static final int TIMEOUT_PAGE_LOAD_SECONDS = 10;
    private static final String TEST_PACKAGE = "org.chromium.chrome.tests";
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String FRAGMENT_TEST_PAGE = "/chrome/test/data/android/fragment.html";
    private static final String TARGET_BLANK_TEST_PAGE =
            "/chrome/test/data/android/cct_target_blank.html";
    private static final String ONLOAD_TITLE_CHANGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        window.onload = function () {"
                    + "            document.title = \"nytimes.com\";"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";
    private static final String DELAYED_TITLE_CHANGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        window.onload = function () {"
                    + "           setTimeout(function (){ document.title = \"nytimes.com\"}, 200);"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static int sIdToIncrement = 1;

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;
    private CustomTabsConnection mConnectionToCleanup;

    @Captor ArgumentCaptor<Intent> mIntentCaptor;

    private static class CustomTabsExtraCallbackHelper<T> extends CallbackHelper {
        private T mValue;

        public T getValue() {
            assert getCallCount() > 0;
            return mValue;
        }

        public void notifyCalled(T value) {
            mValue = value;
            notifyCalled();
        }
    }

    @Rule public final ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mAutomotiveRule.setIsAutomotive(false);
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL);
        pref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);

        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (getActivity() == null) return;
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

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsIntentTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    @Test
    @SmallTest
    public void testAllowedHeadersReceivedWhenConnectionVerified() throws Exception {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage2);

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        intent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                "app1",
                                Origin.create(intent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadUrl(
                                        Tab tab,
                                        LoadUrlParams params,
                                        LoadUrlResult loadUrlResult) {
                                    assertTrue(
                                            params.getVerbatimHeaders()
                                                    .contains("bearer-token: Some token"));
                                    assertTrue(
                                            params.getVerbatimHeaders()
                                                    .contains(
                                                            "redirect-url:"
                                                                    + " https://www.google.com"));
                                    pageLoadFinishedHelper.notifyCalled();
                                }
                            });
                });

        ThreadUtils.runOnUiThreadBlocking(() -> getSessionDataHolder().handleIntent(intent));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    private void addToolbarColorToIntent(Intent intent, int color) {
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, color);
    }

    private Bundle makeBottomBarBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        PendingIntent pi =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true));

        bundle.putInt(CustomTabsIntent.KEY_ID, sIdToIncrement++);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        return bundle;
    }

    private Bundle makeUpdateVisualsBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        return bundle;
    }

    private void openAppMenuAndAssertMenuShown() {
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mCustomTabActivityTestRule.getActivity());
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

    private void setCanUseHiddenTabForSession(
            CustomTabsConnection connection, CustomTabsSessionToken token, boolean useHiddenTab) {
        assert mConnectionToCleanup == null || mConnectionToCleanup == connection;
        // Save the connection. In case the hidden tab is not consumed by the test, ensure that it
        // is properly cleaned up after the test.
        mConnectionToCleanup = connection;
        connection.setCanUseHiddenTabForSession(token, useHiddenTab);
    }

    @Test
    @SmallTest
    public void testContextMenuEntriesBeforeFirstRun() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        // Mark the first run as not completed. This has to be done after we start the intent,
        // otherwise we are going to hit the FRE.
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        // The context menu for images should not be built when the first run is not completed.
        ContextMenuCoordinator imageMenu =
                ContextMenuUtils.openContextMenu(
                        mCustomTabActivityTestRule.getActivity().getActivityTab(), "logo");
        assertNull(
                "Context menu for images should not be built when first run is not finished.",
                imageMenu);

        // Options on the context menu for links should be limited when the first run is not
        // completed.
        ContextMenuCoordinator linkMenu =
                ContextMenuUtils.openContextMenu(
                        mCustomTabActivityTestRule.getActivity().getActivityTab(), "aboutLink");
        final int expectedMenuItems = 4;
        Assert.assertEquals(
                "Menu item count does not match expectation.",
                expectedMenuItems,
                linkMenu.getCount());

        Assert.assertNotNull(linkMenu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(linkMenu.findItem(R.id.contextmenu_copy_link_address));
    }

    /**
     * Test whether the color of the toolbar is correctly customized. For L or later releases,
     * status bar color is also tested.
     */
    @Test
    @SmallTest
    @Feature({"StatusBar"})
    public void testToolbarColor() {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        assertEquals(expectedColor, toolbar.getBackground().getColor());
        Assert.assertFalse(
                mCustomTabActivityTestRule
                        .getActivity()
                        .getToolbarManager()
                        .getLocationBarModelForTesting()
                        .shouldEmphasizeHttpsScheme());
        // Status bar color is constantly black on automotive. See b/285208454.
        int expectedStatusBarColor =
                BuildInfo.getInstance().isAutomotive ? Color.BLACK : expectedColor;
        assertEquals(
                expectedStatusBarColor,
                mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        // TODO(crbug.com/40588353): Use helper class to determine whether dark status icons
        // are supported.

        MenuButton menuButtonView = toolbarView.findViewById(R.id.menu_button_wrapper);
        assertEquals(
                ColorUtils.shouldUseLightForegroundOnBackground(expectedColor)
                        ? BrandedColorScheme.DARK_BRANDED_THEME
                        : BrandedColorScheme.LIGHT_BRANDED_THEME,
                menuButtonView.getBrandedColorSchemeForTesting());
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @DisabledTest(message = "https://crbug.com/40898152")
    public void testActionButton() throws TimeoutException {
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi =
                CustomTabsIntentTestUtils.addActionButtonToIntent(
                        intent, expectedIcon, "Good test", sIdToIncrement++);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity()
                .getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        Assert.assertNotNull(actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue(
                "Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue(
                "Action button does not have the correct bitmap.",
                expectedIcon.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Action Buttons");

        ThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished.waitForCallback("Pending Intent was not sent.");
        assertThat(onFinished.getCallbackIntent().getDataString(), equalTo(mTestPage));
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @DisabledTest(message = "https://crbug.com/40898152")
    public void testMultipleActionButtons() throws TimeoutException {
        Bitmap expectedIcon1 = createVectorDrawableBitmap(R.drawable.ic_content_copy_black, 48, 48);
        Bitmap expectedIcon2 =
                createVectorDrawableBitmap(R.drawable.ic_email_googblue_36dp, 48, 48);
        Intent intent = createMinimalCustomTabIntent();

        // Mark the intent as trusted so it can show more than one action button.
        IntentUtils.addTrustedIntentExtras(intent);
        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(intent));

        ArrayList<Bundle> toolbarItems = new ArrayList<>(2);
        final PendingIntent pi1 =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true));
        final OnFinishedForTest onFinished1 = new OnFinishedForTest(pi1);
        toolbarItems.add(
                CustomTabsIntentTestUtils.makeToolbarItemBundle(
                        expectedIcon1, "Good test", pi1, sIdToIncrement++));
        final PendingIntent pi2 =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        1,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true));
        assertThat(pi2, not(equalTo(pi1)));
        final OnFinishedForTest onFinished2 = new OnFinishedForTest(pi2);
        toolbarItems.add(
                CustomTabsIntentTestUtils.makeToolbarItemBundle(
                        expectedIcon2, "Even gooder test", pi2, sIdToIncrement++));
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, toolbarItems);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Forward the onFinished event to both objects.
        getActivity()
                .getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(
                        (pendingIntent, openedIntent, resultCode, resultData, resultExtras) -> {
                            onFinished1.onSendFinished(
                                    pendingIntent,
                                    openedIntent,
                                    resultCode,
                                    resultData,
                                    resultExtras);
                            onFinished2.onSendFinished(
                                    pendingIntent,
                                    openedIntent,
                                    resultCode,
                                    resultData,
                                    resultExtras);
                        });

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(1);

        Assert.assertNotNull("Action button not found", actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue(
                "Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue(
                "Action button does not have the correct bitmap.",
                expectedIcon1.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Multiple Action Buttons");

        ThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished1.waitForCallback("Pending Intent was not sent.");
        assertThat(onFinished1.getCallbackIntent().getDataString(), equalTo(mTestPage));
        assertNull(onFinished2.getCallbackIntent());

        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        int id = toolbarItems.get(0).getInt(CustomTabsIntent.KEY_ID);
        Bundle updateActionButtonBundle =
                makeUpdateVisualsBundle(id, expectedIcon2, "Bestest testest");
        Bundle updateVisualsBundle = new Bundle();
        updateVisualsBundle.putParcelableArrayList(
                CustomTabsIntent.EXTRA_TOOLBAR_ITEMS,
                new ArrayList<>(Arrays.asList(updateActionButtonBundle)));
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.updateVisuals(token, updateVisualsBundle));

        assertEquals("Bestest testest", actionButton.getContentDescription());
    }

    /**
     * Test the case that the action button should not be shown, given a bitmap with unacceptable
     * height/width ratio.
     */
    @Test
    @SmallTest
    public void testActionButtonBadRatio() {
        Bitmap expectedIcon = createTestBitmap(60, 20);
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsIntentTestUtils.setShareState(intent, CustomTabsIntent.SHARE_STATE_OFF);
        CustomTabsIntentTestUtils.addActionButtonToIntent(
                intent, expectedIcon, "Good test", sIdToIncrement++);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        assertNull("Action button should not be shown", actionButton);

        BrowserServicesIntentDataProvider dataProvider = getActivity().getIntentDataProvider();
        assertThat(dataProvider.getCustomButtonsOnToolbar(), is(empty()));
    }

    @Test
    @SmallTest
    public void testBottomBar() {
        final int numItems = 3;
        final Bitmap expectedIcon = createTestBitmap(48, 24);
        final int barColor = Color.GREEN;

        Intent intent = createMinimalCustomTabIntent();
        ArrayList<Bundle> bundles = new ArrayList<>();
        for (int i = 1; i <= numItems; i++) {
            Bundle bundle = makeBottomBarBundle(i, expectedIcon, Integer.toString(i));
            bundles.add(bundle);
        }
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, bundles);
        intent.putExtra(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR, barColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ViewGroup bottomBar =
                mCustomTabActivityTestRule
                        .getActivity()
                        .findViewById(R.id.custom_tab_bottom_bar_wrapper);
        Assert.assertNotNull(bottomBar);
        Assert.assertTrue(
                "Bottom Bar wrapper is not visible.",
                bottomBar.getVisibility() == View.VISIBLE
                        && bottomBar.getHeight() > 0
                        && bottomBar.getWidth() > 0);
        assertEquals(
                "Bottom Bar showing incorrect number of buttons.",
                numItems,
                bottomBar.getChildCount());
        assertEquals(
                "Bottom bar not showing correct color",
                barColor,
                ((ColorDrawable) bottomBar.getBackground()).getColor());
        for (int i = 0; i < numItems; i++) {
            ImageButton button = (ImageButton) bottomBar.getChildAt(i);
            Assert.assertTrue(
                    "Bottom Bar button does not have the correct bitmap.",
                    expectedIcon.sameAs(((BitmapDrawable) button.getDrawable()).getBitmap()));
            Assert.assertTrue(
                    "Bottom Bar button is not visible.",
                    button.getVisibility() == View.VISIBLE
                            && button.getHeight() > 0
                            && button.getWidth() > 0);
            assertEquals(
                    "Bottom Bar button does not have correct content description",
                    Integer.toString(i + 1),
                    button.getContentDescription());
        }
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testRemoteViews() {
        Intent intent = createMinimalCustomTabIntent();

        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        CustomTabsIntentTestUtils.addActionButtonToIntent(
                intent, expectedIcon, "Good test", sIdToIncrement++);

        // Create a RemoteViews. The layout used here is pretty much arbitrary, but with the
        // constraint that a) it already exists in production code, and b) it only contains views
        // with the @RemoteView annotation.
        RemoteViews remoteViews =
                new RemoteViews(
                        ApplicationProvider.getApplicationContext().getPackageName(),
                        R.layout.share_sheet_item);
        remoteViews.setTextViewText(R.id.text, "Kittens!");
        remoteViews.setTextViewText(R.id.display_new, "So fluffy");
        remoteViews.setImageViewResource(R.id.icon, R.drawable.ic_email_googblue_36dp);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS, remoteViews);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS, new int[] {R.id.icon});
        PendingIntent pi2 =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true));
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT, pi2);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        mScreenShooter.shoot("Remote Views");
    }

    @Test
    @SmallTest
    public void testLaunchWithSession() throws Exception {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
    }

    @Test
    @SmallTest
    public void testRecordRetainableSession_WithCctSession() throws Exception {
        Activity emptyActivity = startBlankUiTestActivity();

        // Write shared pref as it there's a previous CCT launch with sessions.
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, mTestPage);
        pref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, TEST_PACKAGE);
        pref.writeInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID, emptyActivity.getTaskId());
        pref.writeLong(
                ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP, SystemClock.uptimeMillis());
        pref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, true);

        Intent intent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mTestPage,
                        false,
                        builder -> {});
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        intent.setData(Uri.parse(mTestPage));

        CustomTabActivity cctActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> emptyActivity.startActivity(intent));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();

        assertLastLaunchedClientAppRecorded(
                ClientIdentifierType.PACKAGE_NAME,
                TEST_PACKAGE,
                mTestPage,
                cctActivity.getTaskId(),
                true);
    }

    @Test
    @SmallTest
    public void testRecordRetainableSession_WithoutWarmupAndSession() {
        Context context = ApplicationProvider.getApplicationContext();
        Activity emptyActivity = startBlankUiTestActivity();

        Intent intent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                                context, mTestPage, /* launchAsNewTask= */ false, builder -> {})
                        .putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, context.getPackageName());

        CustomTabActivity cctActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> emptyActivity.startActivity(intent));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();

        assertLastLaunchedClientAppRecorded(
                ClientIdentifierType.REFERRER, "", mTestPage, cctActivity.getTaskId(), false);

        Activity activity = getActivity();
        activity.finish();
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);

        // Write shared prefs as it the last CCT session has saw tab interactions.
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        pref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, true);

        // Start another CCT with same intent right away.
        Intent newIntent = new Intent(intent);
        cctActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> emptyActivity.startActivity(newIntent));
        mCustomTabActivityTestRule.setActivity(cctActivity);
        mCustomTabActivityTestRule.waitForActivityCompletelyLoaded();
        assertLastLaunchedClientAppRecorded(
                ClientIdentifierType.REFERRER, "", mTestPage, getActivity().getTaskId(), true);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1308065")
    public void testLoadNewUrlWithSession() throws Exception {
        final Context context = ApplicationProvider.getApplicationContext();
        final Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        Assert.assertFalse(
                "CustomTabContentHandler handled intent with wrong session",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return getSessionDataHolder()
                                    .handleIntent(
                                            CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                                    context, mTestPage2));
                        }));
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                            is(mTestPage));
                });
        Assert.assertTrue(
                "CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            intent.setData(Uri.parse(mTestPage2));
                            return getSessionDataHolder().handleIntent(intent);
                        }));
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onPageLoadFinished(Tab tab, GURL url) {
                                    pageLoadFinishedHelper.notifyCalled();
                                }
                            });
                });
        pageLoadFinishedHelper.waitForCallback(0);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                            is(mTestPage2));
                });
    }

    @Test
    @SmallTest
    public void testCreateNewTab() throws Exception {
        final String testUrl =
                mTestServer.getURL("/chrome/test/data/android/customtabs/test_window_open.html");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), testUrl));
        final TabModelSelector tabSelector =
                mCustomTabActivityTestRule.getActivity().getTabModelSelector();

        final CallbackHelper openTabHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabSelector
                            .getModel(false)
                            .addObserver(
                                    new TabModelObserver() {
                                        @Override
                                        public void didAddTab(
                                                Tab tab,
                                                @TabLaunchType int type,
                                                @TabCreationState int creationState,
                                                boolean markedForSelection) {
                                            openTabHelper.notifyCalled();
                                        }
                                    });
                });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "new_window");

        openTabHelper.waitForCallback(0, 1);
        assertEquals(
                "A new tab should have been created.", 2, tabSelector.getModel(false).getCount());
    }

    @Test
    @SmallTest
    public void testReferrerAddedAutomatically() throws Exception {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        assertEquals(referrer, connection.getDefaultReferrerForSession(session).getUrl());

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadUrl(
                                        Tab tab,
                                        LoadUrlParams params,
                                        LoadUrlResult loadUrlResult) {
                                    assertEquals(referrer, params.getReferrer().getUrl());
                                }

                                @Override
                                public void onPageLoadFinished(Tab tab, GURL url) {
                                    pageLoadFinishedHelper.notifyCalled();
                                }
                            });
                });
        Assert.assertTrue(
                "CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getSessionDataHolder().handleIntent(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testVerifiedReferrer() throws Exception {
        final Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        String referrer = "https://example.com";
        intent.putExtra(Intent.EXTRA_REFERRER_NAME, referrer);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                "app1",
                                Origin.create(referrer),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onLoadUrl(
                                        Tab tab,
                                        LoadUrlParams params,
                                        LoadUrlResult loadUrlResult) {
                                    assertEquals(referrer, params.getReferrer().getUrl());
                                }

                                @Override
                                public void onPageLoadFinished(Tab tab, GURL url) {
                                    pageLoadFinishedHelper.notifyCalled();
                                }
                            });
                });
        Assert.assertTrue(
                "CustomTabContentHandler can't handle intent with same session",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getSessionDataHolder().handleIntent(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    /** Tests that the navigation callbacks are sent. */
    @Test
    @SmallTest
    public void testCallbacksAreSent() throws Exception {
        final Semaphore navigationStartSemaphore = new Semaphore(0);
        final Semaphore navigationFinishedSemaphore = new Semaphore(0);
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onNavigationEvent(
                                            int navigationEvent, Bundle extras) {
                                        Assert.assertNotEquals(
                                                CustomTabsCallback.NAVIGATION_FAILED,
                                                navigationEvent);
                                        Assert.assertNotEquals(
                                                CustomTabsCallback.NAVIGATION_ABORTED,
                                                navigationEvent);
                                        if (navigationEvent
                                                == CustomTabsCallback.NAVIGATION_STARTED) {
                                            navigationStartSemaphore.release();
                                        } else if (navigationEvent
                                                == CustomTabsCallback.NAVIGATION_FINISHED) {
                                            navigationFinishedSemaphore.release();
                                        }
                                    }
                                })
                        .session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(
                navigationStartSemaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
        Assert.assertTrue(
                navigationFinishedSemaphore.tryAcquire(
                        TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
    }

    /** Tests that page load metrics are sent. */
    @Test
    @MediumTest
    public void testPageLoadMetricsAreSent() throws Exception {
        checkPageLoadMetrics(true);
    }

    /** Tests that page load metrics are not sent when the client is not allowlisted. */
    @Test
    @MediumTest
    public void testPageLoadMetricsAreNotSentByDefault() throws Exception {
        checkPageLoadMetrics(false);
    }

    private static void assertSuffixedHistogramTotalCount(long expected, String histogramPrefix) {
        for (String suffix : new String[] {".ZoomedIn", ".ZoomedOut"}) {
            assertEquals(
                    expected,
                    RecordHistogram.getHistogramTotalCountForTesting(histogramPrefix + suffix));
        }
    }

    /**
     * Tests that one navigation in a custom tab records the histograms reflecting time from intent
     * to first navigation start/commit.
     */
    @Test
    @SmallTest
    public void testNavigationHistogramsRecorded() throws Exception {
        String startHistogramPrefix = "CustomTabs.IntentToFirstNavigationStartTime";
        String commitHistogramPrefix = "CustomTabs.IntentToFirstCommitNavigationTime3";
        assertSuffixedHistogramTotalCount(0, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(0, commitHistogramPrefix);

        final Semaphore semaphore = new Semaphore(0);
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onNavigationEvent(
                                            int navigationEvent, Bundle extras) {
                                        if (navigationEvent
                                                == CustomTabsCallback.NAVIGATION_FINISHED) {
                                            semaphore.release();
                                        }
                                    }
                                })
                        .session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));

        assertSuffixedHistogramTotalCount(1, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(1, commitHistogramPrefix);
    }

    /** Tests that TITLE_ONLY state works as expected with a title getting set onload. */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitle() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        final String url = webServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
        webServer.shutdown();
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set during prerendering.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitleHiddenTab() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        final String url = webServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, true, "nytimes.com");
        webServer.shutdown();
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set delayed after load.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithDelayedTitle() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        final String url = webServer.setResponse("/test.html", DELAYED_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
        webServer.shutdown();
    }

    private void hideDomainAndEnsureTitleIsSet(
            final String url, boolean useHiddenTab, final String expectedTitle) throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setHideDomainForSession(token, true);

        if (useHiddenTab) {
            setCanUseHiddenTabForSession(connection, token, true);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(url));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    CustomTabToolbar toolbar =
                            mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
                    TextView titleBar = toolbar.findViewById(R.id.title_bar);
                    Criteria.checkThat(titleBar, Matchers.notNullValue());
                    Criteria.checkThat(titleBar.isShown(), is(true));
                    Criteria.checkThat(titleBar.getText().toString(), is(expectedTitle));
                });
    }

    /**
     * Tests that when we use a pre-created renderer, the page loaded is the only one in the
     * navigation history.
     */
    @Test
    @SmallTest
    public void testPrecreatedRenderer() throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        // Forcing no hidden tab implies falling back to simply creating a spare WebContents.
        setCanUseHiddenTabForSession(connection, token, false);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });

        Assert.assertFalse(mCustomTabActivityTestRule.getActivity().getActivityTab().canGoBack());
        Assert.assertFalse(
                mCustomTabActivityTestRule.getActivity().getActivityTab().canGoForward());

        List<HistoryItem> history =
                getHistory(mCustomTabActivityTestRule.getActivity().getActivityTab());
        assertEquals(1, history.size());
        assertEquals(mTestPage, history.get(0).getUrl().getSpec());
    }

    /** Tests that calling warmup() is optional without prerendering. */
    @Test
    @SmallTest
    public void testMayLaunchUrlWithoutWarmupNoSpeculation() {
        mayLaunchUrlWithoutWarmup(false);
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @Test
    @SmallTest
    public void testMayLaunchUrlWithoutWarmupHiddenTab() {
        mayLaunchUrlWithoutWarmup(true);
    }

    @Test
    @LargeTest
    @EnableFeatures({
        ChromeFeatureList.PREFETCH_BROWSER_INITIATED_TRIGGERS,
        ChromeFeatureList.CCT_NAVIGATIONAL_PREFETCH
    })
    public void testNavigationalPrefetch() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);

        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);

        // Start prefetch and wait until the prefetch request is completed.
        connection.prefetch(
                token, List.of(Uri.parse(mTestPage)), new PrefetchOptions.Builder().build());
        PrefetchTestUtil.waitUntilPrefetchResponseCompleted(new GURL(mTestPage));

        // Open custom tabs, and check prefetch result is served as expected.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Preloading.Prefetch.Attempt.ChromeCustomTabs.TriggeringOutcome",
                                /*kSuccess*/ 5)
                        .build();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertEquals(mTestPage, ChromeTabUtils.getUrlStringOnUiThread(tab));
        histogramWatcher.assertExpected();
    }

    /**
     * Tests that launching a regular Chrome tab after warmup() gives the right layout.
     *
     * <p>About the restrictions and switches: No FRE and no document mode to get a
     * ChromeTabbedActivity, and no tablets to have the tab switcher button.
     *
     * <p>Non-regression test for crbug.com/547121. @SmallTest Disabled for flake:
     * https://crbug.com/692025.
     */
    @Test
    @DisabledTest(message = "crbug.com/692025")
    @Restriction(DeviceFormFactor.PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRegularChrome() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        Intent intent =
                new Intent(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                getInstrumentation().addMonitor(ChromeTabbedActivity.class.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity)
                        monitor.waitForActivityWithTimeout(
                                ChromeActivityTestRule.getActivityStartTimeoutMs());
        Assert.assertNotNull("ChromeTabbedActivity did not start", tabbedActivity);
        Assert.assertNotNull(
                "Should have a tab switcher button.",
                tabbedActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests that launching a Custom Tab after warmup() gives the right layout.
     *
     * <p>Non-regression test for crbug.com/547121.
     */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRightToolbarLayout() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mCustomTabActivityTestRule.startActivityCompletely(createMinimalCustomTabIntent());
        assertNull(
                "Should not have a tab switcher button.",
                getActivity().findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests the following scenario: - warmup() +
     * mayLaunchUrl("http://example.com/page.html#first-fragment") -
     * loadUrl("http://example.com/page.html#other-fragment")
     *
     * <p>The expected behavior is that the hidden tab shouldn't be dropped, and that the fragment
     * is updated.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(true, true);
    }

    /** Same as above, but the hidden tab matching should not ignore the fragment. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentDontIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(false, true);
    }

    /** Same as above, hidden tab matching ignores the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @DisabledTest(message = "https://crbug.com/1148544")
    public void testHiddenTabAndChangingFragmentDontWait() throws Exception {
        startHiddenTabAndChangeFragment(true, false);
    }

    /** Same as above, hidden tab matching doesn't ignore the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentDontWaitDrop() throws Exception {
        startHiddenTabAndChangeFragment(false, false);
    }

    @Test
    @SmallTest
    public void testParallelRequest() throws Exception {
        String url = mTestServer.getURL("/echoheader?Cookie");
        Uri requestUri = Uri.parse(mTestServer.getURL("/set-cookie?acookie"));

        final Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        // warmup(), create session, allow parallel requests, allow origin.
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final Origin origin = Origin.create(requestUri);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setAllowParallelRequestForSession(token, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeOriginVerifier.addVerificationOverride(
                            context.getPackageName(),
                            origin,
                            CustomTabsService.RELATION_USE_AS_ORIGIN);
                });

        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, requestUri);
        intent.putExtra(
                CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, Uri.parse(origin.toString()));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        assertEquals("\"acookie\"", content);
    }

    /**
     * Tests the following scenario: - warmup() +
     * mayLaunchUrl("http://example.com/page.html#first-fragment") -
     * loadUrl("http://example.com/page.html#other-fragment")
     *
     * <p>There are two parameters changing the behavior:
     *
     * @param ignoreFragments Whether the hidden tab should be kept.
     * @param wait Whether to wait for the hidden tab to load.
     */
    private void startHiddenTabAndChangeFragment(boolean ignoreFragments, boolean wait)
            throws Exception {
        String testUrl = mTestServer.getURL(FRAGMENT_TEST_PAGE);
        String initialFragment = "#test";
        String initialUrl = testUrl + initialFragment;
        String fragment = "#yeah";
        String urlWithFragment = testUrl + fragment;

        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, urlWithFragment);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setIgnoreUrlFragmentsForSession(token, ignoreFragments);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(initialUrl), null, null));

        if (wait) {
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, initialUrl);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = getActivity().getActivityTab();

        if (wait) {
            ElementContentCriteria initialVisibilityCriteria =
                    new ElementContentCriteria(
                            tab, "visibility", ignoreFragments ? "hidden" : "visible");
            ElementContentCriteria initialFragmentCriteria =
                    new ElementContentCriteria(
                            tab, "initial-fragment", ignoreFragments ? initialFragment : fragment);
            ElementContentCriteria fragmentCriteria =
                    new ElementContentCriteria(tab, "fragment", fragment);
            // The tab hasn't been reloaded.
            CriteriaHelper.pollInstrumentationThread(initialVisibilityCriteria, 2000, 200);
            // No reload (initial fragment is correct).
            CriteriaHelper.pollInstrumentationThread(initialFragmentCriteria, 2000, 200);
            if (ignoreFragments) {
                CriteriaHelper.pollInstrumentationThread(fragmentCriteria, 2000, 200);
            }
        } else {
            CriteriaHelper.pollInstrumentationThread(
                    new ElementContentCriteria(tab, "initial-fragment", fragment), 2000, 200);
        }

        Assert.assertFalse(tab.canGoForward());
        Assert.assertFalse(tab.canGoBack());

        // TODO(crbug.com/40276951):
        // Fragment misses will trigger two history entries
        // - url#speculated and url#actual are both inserted
        // This should ideally not be the case.
    }

    /**
     * Test whether the url shown on hidden tab gets updated from about:blank when it completes in
     * the background.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabCorrectUrl() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, mTestPage);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        assertEquals(
                Uri.parse(mTestPage).getHost() + ":" + Uri.parse(mTestPage).getPort(),
                ((EditText) getActivity().findViewById(R.id.url_bar)).getText().toString());
    }

    /** Test that hidden tab speculation is not performed if 3rd party cookies are blocked. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    public void testHiddenTabThirdPartyCookiesBlocked() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        connection.warmup(0);

        // Needs the browser process to be initialized.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefs = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    int old_block_pref = prefs.getInteger(COOKIE_CONTROLS_MODE);
                    prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.OFF);
                    Assert.assertTrue(connection.maySpeculate(token));
                    prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.BLOCK_THIRD_PARTY);
                    Assert.assertFalse(connection.maySpeculate(token));
                    prefs.setInteger(COOKIE_CONTROLS_MODE, old_block_pref);
                });
    }

    /** Test whether invalid urls are avoided for hidden tab. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabInvalidUrl() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse("chrome://version"), null, null));
    }

    /**
     * Tests that the activity knows there is already a child process when warmup() has been called.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionWithWarmup() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Warmup() should have allocated a child connection",
                                getActivity().shouldAllocateChildConnection()));
    }

    /** Tests that the activity knows there is no child process. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionNoWarmup() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = CustomTabsConnection.getInstance();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "No spare renderer available, should allocate a child connection.",
                                getActivity().shouldAllocateChildConnection()));
    }

    /** Tests that the activity knows there is already a child process with a hidden tab. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionWithHiddenTab() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Prerendering should have allocated a child connection",
                                getActivity().shouldAllocateChildConnection()));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecreateSpareRendererOnTabClose() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsTestUtils.warmUpAndWait();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
                    final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
                    activity.getComponent()
                            .resolveNavigationController()
                            .finish(FinishReason.OTHER);
                });
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_PREWARM_TAB)) {
            CriteriaHelper.pollUiThread(
                    () ->
                            WarmupManager.getInstance()
                                    .hasSpareTab(ProfileManager.getLastUsedRegularProfile()),
                    "No new spare tab");
        } else {
            CriteriaHelper.pollUiThread(
                    () -> WarmupManager.getInstance().hasSpareWebContents(),
                    "No new spare renderer");
        }
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testInteractionRecordedOnClose() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
                    activity.finish();
                });
        CriteriaHelper.pollUiThread(() -> getActivity().isDestroyed());

        Assert.assertTrue(
                "CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION not recorded.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION));
        Assert.assertTrue(
                "CUSTOM_TABS_LAST_CLOSE_TIMESTAMP not recorded.",
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "CustomTabs.HadInteractionOnClose.Form"));
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is not lost when launching the Custom
     * Tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabWithReferrer() throws Exception {
        String referrer = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, referrer, referrer);

        Tab tab = getActivity().getActivityTab();
        // The tab hasn't been reloaded.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "hidden"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrer), 2000, 200);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is dropped when the tab is launched
     * with a mismatched referrer.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabWithMismatchedReferrers() throws Exception {
        String prerenderReferrer = "android-app://com.foo.me/";
        String launchReferrer = "android-app://com.foo.me.i.changed.my.mind/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, prerenderReferrer, launchReferrer);

        Tab tab = getActivity().getActivityTab();
        // Prerender has been dropped.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "visible"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, launchReferrer), 2000, 200);
    }

    /** Tests that a client can set a referrer, without speculating. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testClientCanSetReferrer() throws Exception {
        String referrerUrl = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(mTestPage, false, null, referrerUrl);

        Tab tab = getActivity().getActivityTab();
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrerUrl), 2000, 200);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "see crbug.com/1361534")
    public void testLaunchIncognitoURL() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
        final CallbackHelper mCctHiddenCallback = new CallbackHelper();
        final CallbackHelper mTabbedModeShownCallback = new CallbackHelper();
        final AtomicReference<ChromeTabbedActivity> tabbedActivity = new AtomicReference<>();

        ActivityStateListener listener =
                (activity, newState) -> {
                    if (activity == cctActivity
                            && (newState == ActivityState.STOPPED
                                    || newState == ActivityState.DESTROYED)) {
                        mCctHiddenCallback.notifyCalled();
                    }

                    if (activity instanceof ChromeTabbedActivity
                            && newState == ActivityState.RESUMED) {
                        mTabbedModeShownCallback.notifyCalled();
                        tabbedActivity.set((ChromeTabbedActivity) activity);
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.registerStateListenerForAllActivities(listener));

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    TabTestUtils.openNewTab(
                            cctActivity.getActivityTab(),
                            new GURL("about:blank"),
                            null,
                            null,
                            WindowOpenDisposition.OFF_THE_RECORD,
                            false);
                });

        mCctHiddenCallback.waitForCallback("CCT not hidden.", 0);
        mTabbedModeShownCallback.waitForCallback("Tabbed mode not shown.", 0);

        CriteriaHelper.pollUiThread(() -> tabbedActivity.get().areTabModelsInitialized());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = tabbedActivity.get().getActivityTab();
                    Criteria.checkThat("Tab is null", tab, Matchers.notNullValue());
                    Criteria.checkThat("Incognito tab not selected", tab.isIncognito(), is(true));
                    Criteria.checkThat(
                            "Wrong URL loaded in incognito tab",
                            ChromeTabUtils.getUrlStringOnUiThread(tab),
                            is("about:blank"));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.unregisterActivityStateListener(listener));
    }

    /** Maybe prerenders a URL with a referrer, then launch it with another one. */
    private void maybeSpeculateAndLaunchWithReferrers(
            String url, boolean useHiddenTab, String speculationReferrer, String launchReferrer)
            throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        if (useHiddenTab) {
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            connection.newSession(token);
            setCanUseHiddenTabForSession(connection, token, true);
            Bundle extras = null;
            if (speculationReferrer != null) {
                extras = new Bundle();
                extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(speculationReferrer));
            }
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), extras, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);
        }

        if (launchReferrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(launchReferrer));
        }
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * Test intended to verify the way we test history is correct. Start an activity and then
     * navigate to a different url. We test NavigationController behavior through canGoBack/Forward
     * as well as browser history through an HistoryProvider.
     */
    @Test
    @SmallTest
    public void testHistoryNoSpeculation() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        Assert.assertTrue(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory(tab);
        assertEquals(2, history.size());
        assertEquals(mTestPage2, history.get(0).getUrl().getSpec());
        assertEquals(mTestPage, history.get(1).getUrl().getSpec());
    }

    /**
     * The following test that history only has a single final page after speculation, whether it
     * was a hit or a miss.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabHit() throws Exception {
        verifyHistoryAfterHiddenTab(true);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabMiss() throws Exception {
        verifyHistoryAfterHiddenTab(false);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1148544")
    public void closeButton_navigatesToLandingPage() throws TimeoutException {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context, mTestServer.getURL(TARGET_BLANK_TEST_PAGE));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        Assert.assertEquals(activity.getCurrentTabModel().getCount(), 1);

        // The link we click will take us to another page. Set the initial page as the landing page.
        activity.getComponent()
                .resolveNavigationController()
                .setLandingPageOnCloseCriterion(url -> url.contains(TARGET_BLANK_TEST_PAGE));

        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), "target_blank_link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(2)));

        ClickUtils.clickButton(activity.findViewById(R.id.close_button));

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(1)));
        assertFalse(activity.isFinishing());
    }

    @Test
    @SmallTest
    public void closeButton_closesActivityIfNoLandingPage() throws TimeoutException {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context, mTestServer.getURL(TARGET_BLANK_TEST_PAGE));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        Assert.assertEquals(activity.getCurrentTabModel().getCount(), 1);

        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), "target_blank_link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(2)));

        ClickUtils.clickButton(activity.findViewById(R.id.close_button));

        CriteriaHelper.pollUiThread(activity::isFinishing);
    }

    // The flag is necessary to ensure the experiment id 101 is honored.
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=ExternalExperimentAllowlist:101/x",
    })
    public void testExperimentIds() throws Exception {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        intent.setData(Uri.parse(mTestPage));
        int[] ids = {101};
        intent.putExtra(CustomTabIntentDataProvider.EXPERIMENT_IDS, ids);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(
                token, "com.google.android.googlequicksearchbox");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(CustomTabsTestUtils.hasVariationId(101));
                });
    }

    // The flag is necessary to ensure the experiment id 101 is honored.
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=ExternalExperimentAllowlist:101/x",
    })
    public void testExperimentIdsFromMayLaunchUrl() throws Exception {
        CallbackHelper warmupHelper = new CallbackHelper();
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onWarmupCompleted(Bundle extras) {
                                        warmupHelper.notifyCalled();
                                    }
                                })
                        .session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(
                token, "com.google.android.googlequicksearchbox");
        Bundle extrasBundle = new Bundle();
        int[] ids = {101};
        extrasBundle.putIntArray(CustomTabIntentDataProvider.EXPERIMENT_IDS, ids);
        Assert.assertTrue(
                connection.mayLaunchUrl(
                        token, Uri.parse("https://www.google.com"), extrasBundle, null));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(CustomTabsTestUtils.hasVariationId(101));
                });

        warmupHelper.waitForCallback(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WarmupManager.getInstance().destroySpareTab();
                });
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "ignore-certificate-errors",
        "ignore-google-port-numbers"
    })
    public void testMayLaunchUrlAddsClientDataHeader() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        webServer.setServerHost("www.google.com");
        final String expectedHeader = "test-header";
        String url = webServer.setResponse("/ok.html", "<html>ok</html>", null);
        ServiceLoaderUtil.setInstanceForTesting(
                CustomTabsConnection.class,
                new CustomTabsConnection() {
                    @Override
                    public void setClientDataHeaderForNewTab(
                            CustomTabsSessionToken session, WebContents webContents) {
                        setClientDataHeader(webContents, expectedHeader);
                    }
                });
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            connection.getHiddenTabForTesting(), Matchers.notNullValue());
                });
        Tab hiddenTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return connection.getHiddenTabForTesting();
                        });
        ChromeTabUtils.waitForTabPageLoaded(hiddenTab, url);
        String actualHeader = webServer.getLastRequest("/ok.html").headerValue("X-CCT-Client-Data");
        assertEquals(expectedHeader, actualHeader);
        webServer.shutdown();
    }

    @Test
    @SmallTest
    public void testCanHideBrowserControls_notPartial() throws Exception {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        assertOverlayPanelCanHideAndroidBrowserControls(true);
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    public void testLaunchPartialCustomTabActivity_BottomSheet() throws Exception {
        doTestLaunchPartialCustomTabWithInitialHeight();
    }

    private void doTestLaunchPartialCustomTabWithInitialHeight() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 50);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // A Normal CCT height is set to MATCH_PARENT while Partial CCT has non-zero value.
        int fullHeight = ViewGroup.LayoutParams.MATCH_PARENT;
        WindowManager.LayoutParams attrs = getActivity().getWindow().getAttributes();
        assertNotEquals("The window should have non-full height", fullHeight, attrs.height);

        assert (mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting()
                instanceof BaseCustomTabRootUiCoordinator);
        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator)
                        mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting();

        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();
        assertEquals(
                "Incorrect strategy type with display manager.",
                PartialCustomTabBaseStrategy.PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());

        // Verify the hierarchy of the enclosing layouts that PCCT relies on for its operation.
        CallbackHelper eventHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity()
                            .getCompositorViewHolderSupplier()
                            .addObserver(
                                    cvh -> {
                                        if (cvh == null) return;
                                        assertTrue(
                                                "CoordinatorLayoutForPointer should be the parent"
                                                        + " of CompositorViewHolder",
                                                cvh.getParent()
                                                        instanceof CoordinatorLayoutForPointer);
                                        assertTrue(
                                                "ContentFrameLayout should be the parent of"
                                                        + " CoodinatorLayoutForPointer",
                                                cvh.getParent().getParent()
                                                        instanceof ContentFrameLayout);
                                        assertNotEquals(
                                                "The window should have non-zero y", 0, attrs.y);
                                        eventHelper.notifyCalled();
                                    });
                });
        eventHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
    public void testLaunchPartialCustomTabActivity_SideSheet() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, 100);
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 300);
        intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_ENABLE_MAXIMIZATION, true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        rotateCustomTabActivity(
                mCustomTabActivityTestRule.getActivity(), Layout.Orientation.LANDSCAPE);

        assert (mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting()
                instanceof BaseCustomTabRootUiCoordinator);
        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator)
                        mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        assert (coordinator.getCustomTabSizeStrategyForTesting()
                instanceof PartialCustomTabDisplayManager);
        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();
        if (displayManager.getActiveStrategyType()
                != PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET) {
            return;
        }

        onViewWaiting(allOf(withId(R.id.custom_tabs_sidepanel_maximize), isDisplayed()));
        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        ImageButton maximizeButton = toolbar.getMaximizeButtonForTest();
        Assert.assertNotNull(maximizeButton);

        // A Normal CCT width is set to MATCH_PARENT while Partial CCT has non-zero value.
        int fullWidth = ViewGroup.LayoutParams.MATCH_PARENT;
        WindowManager.LayoutParams attrs = getActivity().getWindow().getAttributes();
        assertNotEquals("The window should have non-full width", fullWidth, attrs.width);

        int maxWidth = getActivity().getResources().getDisplayMetrics().widthPixels;
        ThreadUtils.runOnUiThreadBlocking((Runnable) maximizeButton::performClick);
        assertEquals("The window should be max width", maxWidth, attrs.width);

        ThreadUtils.runOnUiThreadBlocking((Runnable) maximizeButton::performClick);
        onViewWaiting(allOf(withId(R.id.custom_tabs_sidepanel_maximize), isDisplayed()));

        assertNotEquals("The window should not be max width", maxWidth, attrs.width);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    // Screen rotation is not relevant on automotive.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
    // Bug in O that's been fixed in 8.1
    // https://issuetracker.google.com/issues/68427483
    public void testLaunchPartialCustomTabActivity_Transition() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, 600);
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 300);
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 300);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        rotateCustomTabActivity(
                mCustomTabActivityTestRule.getActivity(), Layout.Orientation.PORTRAIT);

        DisplayMetrics displayMetrics = getActivity().getResources().getDisplayMetrics();
        int devicePortraitWidthDp = (int) (displayMetrics.widthPixels / displayMetrics.density);
        int deviceLandscapeWidthDp = (int) (displayMetrics.heightPixels / displayMetrics.density);

        assert (mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting()
                instanceof BaseCustomTabRootUiCoordinator);
        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator)
                        mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        assert (coordinator.getCustomTabSizeStrategyForTesting()
                instanceof PartialCustomTabDisplayManager);
        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();

        assertSideSheetOrBottomSheet(displayManager, devicePortraitWidthDp);
        rotateCustomTabActivity(
                mCustomTabActivityTestRule.getActivity(), Layout.Orientation.LANDSCAPE);
        assertSideSheetOrBottomSheet(displayManager, deviceLandscapeWidthDp);
        rotateCustomTabActivity(
                mCustomTabActivityTestRule.getActivity(), Layout.Orientation.PORTRAIT);
        assertSideSheetOrBottomSheet(displayManager, devicePortraitWidthDp);
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    public void testLaunchPartialCustomTabActivity_FullSize() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.testapp");
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_HEIGHT_PX, 300);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assert (mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting()
                instanceof BaseCustomTabRootUiCoordinator);
        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator)
                        mCustomTabActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        assert (coordinator.getCustomTabSizeStrategyForTesting()
                instanceof PartialCustomTabDisplayManager);
        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();
        int fullWidth = ViewGroup.LayoutParams.MATCH_PARENT;
        int fullHeight = ViewGroup.LayoutParams.MATCH_PARENT;
        WindowManager.LayoutParams attrs = getActivity().getWindow().getAttributes();
        assertEquals("The window should have full width", fullWidth, attrs.width);
        assertEquals("The window should have full height", fullHeight, attrs.height);
        assertEquals(
                "Incorrect strategy type",
                displayManager.getActiveStrategyType(),
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE);
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    public void testLaunchPartialCustomTabActivity_startActivityForResult() {
        CustomTabsIntent customTabsIntent =
                new CustomTabsIntent.Builder().setInitialActivityHeightPx(200).build();
        Intent intent = customTabsIntent.intent;
        intent.setData(Uri.parse("https://example.com"));
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        intent.setPackage(packageName);
        intent.putExtra(EXTRA_INITIAL_ACTIVITY_WIDTH_PX, 300);
        intent.putExtra(EXTRA_ACTIVITY_SIDE_SHEET_BREAKPOINT_DP, 600);
        mChromeTabbedActivityTestRule.startMainActivityOnBlankPage();

        CustomTabActivity resultActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> {
                            mChromeTabbedActivityTestRule
                                    .getActivity()
                                    .startActivityForResult(intent, 0);
                        });

        rotateCustomTabActivity(resultActivity, Layout.Orientation.PORTRAIT);

        DisplayMetrics displayMetrics = resultActivity.getResources().getDisplayMetrics();
        int devicePortraitWidthDp = (int) (displayMetrics.widthPixels / displayMetrics.density);

        assert (resultActivity.getRootUiCoordinatorForTesting()
                instanceof BaseCustomTabRootUiCoordinator);
        BaseCustomTabRootUiCoordinator coordinator =
                (BaseCustomTabRootUiCoordinator) resultActivity.getRootUiCoordinatorForTesting();
        assert (coordinator.getCustomTabSizeStrategyForTesting()
                instanceof PartialCustomTabDisplayManager);
        PartialCustomTabDisplayManager displayManager =
                (PartialCustomTabDisplayManager) coordinator.getCustomTabSizeStrategyForTesting();

        assertSideSheetOrBottomSheet(displayManager, devicePortraitWidthDp);

        Drawable windowBackground = resultActivity.getWindow().getDecorView().getBackground();
        int windowBackgroundColor = ((ColorDrawable) windowBackground).getColor();
        boolean isTranslucent = Color.alpha(windowBackgroundColor) < 255;
        assertTrue(isTranslucent);
    }

    private void doOpaqueOriginTest(boolean prefetch) throws Exception {
        TestWebServer webServer = TestWebServer.start();
        String url = webServer.setResponse("/ok.html", "<html>ok</html>", null);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);

        if (prefetch) {
            setCanUseHiddenTabForSession(connection, token, true);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                connection.getHiddenTabForTesting(), Matchers.notNullValue());
                    });
            Tab hiddenTab =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                return connection.getHiddenTabForTesting();
                            });
            ChromeTabUtils.waitForTabPageLoaded(hiddenTab, url);
        } else {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
        String actualHeader = webServer.getLastRequest("/ok.html").headerValue("Sec-Fetch-Site");
        assertEquals("cross-site", actualHeader);
        webServer.shutdown();
    }

    @Test
    @LargeTest
    public void testOpaqueOriginFromPrefetch_Enabled() throws Exception {
        doOpaqueOriginTest(true);
    }

    @Test
    @LargeTest
    public void testOpaqueOriginFromIntent_Enabled() throws Exception {
        doOpaqueOriginTest(false);
    }

    /** Asserts that the Overlay Panel is set to allow or not allow ever hiding the Toolbar. */
    private void assertOverlayPanelCanHideAndroidBrowserControls(boolean canEverHide) {
        // Wait for CS to get initialized.
        CriteriaHelper.pollUiThread(
                () -> getActivity().getContextualSearchManagerForTesting() != null);

        // The toolbar cannot go away for Partial Height Custom Tabs, but can for full height ones.
        CriteriaHelper.pollUiThread(
                () ->
                        getActivity()
                                        .getContextualSearchManagerForTesting()
                                        .getCanHideAndroidBrowserControls()
                                == canEverHide);
    }

    private void verifyHistoryAfterHiddenTab(boolean speculationWasAHit) throws Exception {
        String speculationUrl = mTestPage;
        String navigationUrl = speculationWasAHit ? mTestPage : mTestPage2;
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, navigationUrl);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);

        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(speculationUrl), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, speculationUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = getActivity().getActivityTab();
        Assert.assertFalse(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory(tab);
        assertEquals(1, history.size());
        assertEquals(navigationUrl, history.get(0).getUrl().getSpec());
    }

    private void mayLaunchUrlWithoutWarmup(boolean useHiddenTab) {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        Bundle extras = null;
        setCanUseHiddenTabForSession(connection, token, useHiddenTab);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertEquals(mTestPage, ChromeTabUtils.getUrlStringOnUiThread(tab));
    }

    private void checkPageLoadMetrics(boolean allowMetrics) throws TimeoutException {
        CustomTabsExtraCallbackHelper<Long> firstContentfulPaintCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Long> largestContentfulPaintCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Long> loadEventStartCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Float> layoutShiftScoreCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Boolean> sawNetworkQualityEstimatesCallback =
                new CustomTabsExtraCallbackHelper<>();

        final AtomicReference<Long> activityStartTimeMs = new AtomicReference<>(-1L);

        CustomTabsCallback cb =
                new CustomTabsCallback() {
                    @Override
                    public void extraCallback(String callbackName, Bundle args) {
                        if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) return;

                        // Check if the callback name is either the Bottom Bar Scroll, or Page Load
                        // Metrics.
                        // See https://crbug.com/963538 for why it might be either.
                        if (!CustomTabsConnection.BOTTOM_BAR_SCROLL_STATE_CALLBACK.equals(
                                callbackName)) {
                            assertEquals(
                                    CustomTabsConnection.PAGE_LOAD_METRICS_CALLBACK, callbackName);
                        }
                        if (-1 != args.getLong(PageLoadMetrics.EFFECTIVE_CONNECTION_TYPE, -1)) {
                            sawNetworkQualityEstimatesCallback.notifyCalled(true);
                        }

                        float layoutShiftScoreValue =
                                args.getFloat(PageLoadMetrics.LAYOUT_SHIFT_SCORE, -1f);
                        if (layoutShiftScoreValue >= 0f) {
                            layoutShiftScoreCallback.notifyCalled(layoutShiftScoreValue);
                        }

                        long navigationStart = args.getLong(PageLoadMetrics.NAVIGATION_START, -1);
                        if (navigationStart == -1) {
                            // Untested metric callback.
                            return;
                        }
                        long current = SystemClock.uptimeMillis();
                        Assert.assertTrue(navigationStart <= current);
                        Assert.assertTrue(navigationStart >= activityStartTimeMs.get());

                        long firstContentfulPaint =
                                args.getLong(PageLoadMetrics.FIRST_CONTENTFUL_PAINT, -1);
                        if (firstContentfulPaint > 0) {
                            Assert.assertTrue(firstContentfulPaint <= (current - navigationStart));
                            firstContentfulPaintCallback.notifyCalled(firstContentfulPaint);
                        }

                        long largestContentfulPaint =
                                args.getLong(PageLoadMetrics.LARGEST_CONTENTFUL_PAINT, -1);
                        if (largestContentfulPaint > 0) {
                            Assert.assertTrue(
                                    largestContentfulPaint <= (current - navigationStart));
                            largestContentfulPaintCallback.notifyCalled(largestContentfulPaint);
                        }

                        long loadEventStart = args.getLong(PageLoadMetrics.LOAD_EVENT_START, -1);
                        if (loadEventStart > 0) {
                            Assert.assertTrue(loadEventStart <= (current - navigationStart));
                            loadEventStartCallback.notifyCalled(loadEventStart);
                        }
                    }
                };

        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(cb).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;

        if (allowMetrics) {
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            connection.mClientManager.setShouldGetPageLoadMetricsForSession(token, true);
        }

        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        activityStartTimeMs.set(SystemClock.uptimeMillis());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        if (allowMetrics) {
            firstContentfulPaintCallback.waitForCallback(0);
            loadEventStartCallback.waitForCallback(0);
            sawNetworkQualityEstimatesCallback.waitForCallback(0);

            assertTrue(firstContentfulPaintCallback.getValue() > 0);
            assertTrue(firstContentfulPaintCallback.getValue() > 0);
            assertTrue(sawNetworkQualityEstimatesCallback.getValue());
        } else {
            try {
                firstContentfulPaintCallback.waitForCallback(0);
                assertTrue(firstContentfulPaintCallback.getValue() > 0);
            } catch (TimeoutException e) {
                // Expected.
            }
            assertEquals(0, firstContentfulPaintCallback.getCallCount());

            try {
                largestContentfulPaintCallback.waitForCallback(0);
                assertTrue(largestContentfulPaintCallback.getValue() > 0);
            } catch (TimeoutException e) {
                // Expected.
            }

            assertEquals(0, largestContentfulPaintCallback.getCallCount());
        }

        // Navigate to a new page, as metrics like LCP are only reported at the end of the page load
        // lifetime.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
                    activity.getComponent()
                            .resolveNavigationController()
                            .navigate(new LoadUrlParams("about:blank"), intent);
                });

        if (allowMetrics) {
            largestContentfulPaintCallback.waitForCallback(0, 1, 15, TimeUnit.SECONDS);
            assertTrue((long) largestContentfulPaintCallback.getValue() > 0);

            layoutShiftScoreCallback.waitForCallback(0);
            assertNotNull(layoutShiftScoreCallback.getValue());
        }
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession(Intent intentWithSession)
            throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intentWithSession);
        connection.newSession(token);
        intentWithSession.setData(Uri.parse(mTestPage));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intentWithSession);
        return token;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession() throws Exception {
        return warmUpAndLaunchUrlWithSession(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage));
    }

    private Activity startBlankUiTestActivity() {
        Context context = ApplicationProvider.getApplicationContext();
        Intent emptyIntent = new Intent(context, BlankUiTestActivity.class);
        emptyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return getInstrumentation().startActivitySync(emptyIntent);
    }

    private static class ElementContentCriteria implements Runnable {
        private final Tab mTab;
        private final String mJsFunction;
        private final String mExpected;

        public ElementContentCriteria(Tab tab, String elementId, String expected) {
            mTab = tab;
            mExpected = "\"" + expected + "\"";
            mJsFunction =
                    "(function () { return document.getElementById(\""
                            + elementId
                            + "\").innerHTML; })()";
        }

        @Override
        public void run() {
            String value = null;
            try {
                String jsonText =
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                mTab.getWebContents(), mJsFunction);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                value = jsonText;
            } catch (TimeoutException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
            Criteria.checkThat("Page element is not as expected.", value, is(mExpected));
        }
    }

    private static List<HistoryItem> getHistory(Tab tab) throws TimeoutException {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = Profile.fromWebContents(tab.getWebContents());
                    BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(profile);
                    historyService.setObserver(historyObserver);
                    String historyQueryFilter = "";
                    historyService.queryHistory(historyQueryFilter, null);
                });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }

    private SessionDataHolder getSessionDataHolder() {
        return ChromeApplicationImpl.getComponent().resolveSessionDataHolder();
    }

    private void assertLastLaunchedClientAppRecorded(
            String histogramSuffix,
            String clientPackage,
            String url,
            int taskId,
            boolean umaRecorded) {
        SharedPreferencesManager pref = ChromeSharedPreferences.getInstance();
        String histogramName =
                "CustomTabs.RetainableSessionsV2.TimeBetweenLaunch" + histogramSuffix;

        Assert.assertEquals(
                "Client package name in shared pref is different.",
                clientPackage,
                pref.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, ""));
        Assert.assertEquals(
                "Url in shared pref is different.",
                url,
                pref.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, ""));
        Assert.assertEquals(
                "Task id in shared pref is different.",
                taskId,
                pref.readInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID, 0));
        Assert.assertEquals(
                String.format("<%s> not recorded correctly.", histogramName),
                umaRecorded ? 1 : 0,
                RecordHistogram.getHistogramTotalCountForTesting(histogramName));
    }

    @Test
    @SmallTest
    public void doesNotLaunchJavaScriptUrls_dispatchToCustomTabActivity() {
        Context context = ApplicationProvider.getApplicationContext();
        String javaScriptUrl = "javascript: alert('Hello');";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent =
                            CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                    context, javaScriptUrl);

                    Activity activity = Mockito.mock(Activity.class);
                    @LaunchIntentDispatcher.Action
                    int result =
                            LaunchIntentDispatcher.dispatchToCustomTabActivity(activity, intent);
                    assertEquals(LaunchIntentDispatcher.Action.CONTINUE, result);
                    verify(activity, never()).startActivity(any(), any());
                });
    }

    @Test
    @SmallTest
    public void doesNotLaunchJavaScriptUrls_dispatch() {
        Context context = ApplicationProvider.getApplicationContext();
        String javaScriptUrl = "javascript: alert('Hello');";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent =
                            CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                    context, javaScriptUrl);

                    Activity activity = Mockito.mock(Activity.class);
                    @LaunchIntentDispatcher.Action
                    int result = LaunchIntentDispatcher.dispatch(activity, intent);
                    assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
                    verify(activity, never()).startActivity(any(), any());
                });
    }

    @Test
    @SmallTest
    public void testCallingActivityExtra() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = createMinimalCustomTabIntent();
                    intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "spoofed");
                    Activity activity = Mockito.mock(Activity.class);

                    when(activity.getCallingPackage()).thenReturn("com.foo.bar");
                    PowerManager powerManager =
                            (PowerManager)
                                    ContextUtils.getApplicationContext()
                                            .getSystemService(Context.POWER_SERVICE);
                    when(activity.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);

                    LaunchIntentDispatcher.dispatch(activity, intent);
                    verify(activity, times(1)).startActivity(mIntentCaptor.capture(), any());

                    Assert.assertEquals(
                            "Calling activity package incorrect.",
                            "com.foo.bar",
                            mIntentCaptor
                                    .getValue()
                                    .getStringExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));
                });
    }

    @Test
    @SmallTest
    public void testCallingActivityExtra_NotSet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = createMinimalCustomTabIntent();
                    intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "spoofed");
                    Activity activity = Mockito.mock(Activity.class);
                    PowerManager powerManager =
                            (PowerManager)
                                    ContextUtils.getApplicationContext()
                                            .getSystemService(Context.POWER_SERVICE);
                    when(activity.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);

                    LaunchIntentDispatcher.dispatch(activity, intent);
                    verify(activity, times(1)).startActivity(mIntentCaptor.capture(), any());

                    Assert.assertNull(
                            "Calling activity package shouldn't be set.",
                            mIntentCaptor
                                    .getValue()
                                    .getStringExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));
                });
    }

    @Test
    @SmallTest
    public void testAppMenuDoesNotShowReadAloud() {
        // TODO(b/292134067): Update when Read Aloud supports Custom Tabs.
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        openAppMenuAndAssertMenuShown();
        Assert.assertNull(
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.readaloud_menu_id));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressNavigationFailure_WithRecover() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = getActivity().getActivityTab();
        BackPressManager.TAB_HISTORY_RECOVER.setForTesting(true);

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        BackPressHandler navigationHandler =
                getActivity()
                        .getBackPressManagerForTesting()
                        .getHandlersForTesting()[BackPressHandler.Type.TAB_HISTORY];
        ObservableSupplierImpl<Boolean> handleBackPressChangedSupplier =
                (ObservableSupplierImpl<Boolean>)
                        navigationHandler.getHandleBackPressChangedSupplier();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.Failure",
                        BackPressManager.getHistogramValue(
                                BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    handleBackPressChangedSupplier.set(false);
                    try {
                        getActivity().getOnBackPressedDispatcher().onBackPressed();
                    } catch (AssertionError ignored) {
                        if (!ignored.getMessage().contains("-1")) throw ignored;
                    }
                });

        histogramWatcher.assertExpected("Failure should be recorded");
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tab should be navigated when tab handler fails",
                            ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                            is(mTestPage));
                });
        BackPressManager.TAB_HISTORY_RECOVER.setForTesting(false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testBackPressNavigationFailure_WithoutRecover() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = getActivity().getActivityTab();
        BackPressManager.TAB_HISTORY_RECOVER.setForTesting(false);

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        BackPressHandler navigationHandler =
                getActivity()
                        .getBackPressManagerForTesting()
                        .getHandlersForTesting()[BackPressHandler.Type.TAB_HISTORY];
        ObservableSupplierImpl<Boolean> handleBackPressChangedSupplier =
                (ObservableSupplierImpl<Boolean>)
                        navigationHandler.getHandleBackPressChangedSupplier();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    handleBackPressChangedSupplier.set(false);
                    try {
                        getActivity().getOnBackPressedDispatcher().onBackPressed();
                    } catch (AssertionError ignored) {
                        if (!ignored.getMessage().contains("-1")) throw ignored;
                    }
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Tab should not be navigated when tab handler fails without recover",
                            ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                            is(mTestPage2));
                });
    }

    @Test
    @SmallTest
    public void disableShareEntriesForAutomotive() {
        mAutomotiveRule.setIsAutomotive(true);
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsIntentTestUtils.setShareState(intent, SHARE_STATE_ON);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ViewGroup toolbarButtons =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.action_buttons);
        Assert.assertEquals(
                "No action buttons should be added.", 0, toolbarButtons.getChildCount());

        openAppMenuAndAssertMenuShown();
        Assert.assertNull(
                "Share option should be hidden.",
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.share_row_menu_id));
    }

    @Test
    @MediumTest
    public void omniboxInCCT_testInteractiveOmniboxOnEligibleCCTs() throws Exception {
        // Permit Omnibox for any upcoming intent(s).
        var connection = Mockito.spy(CustomTabsConnection.getInstance());
        doReturn(true).when(connection).shouldEnableOmniboxForIntent(any());
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        var tab = getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage);

        var titleBar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.title_url_container);
        Assert.assertTrue(titleBar.hasOnClickListeners());

        var url = mCustomTabActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(
                "Url bar should have an accessibility delegate.", url.getAccessibilityDelegate());

        UserActionTester userActionTester = new UserActionTester();
        ThreadUtils.runOnUiThreadBlocking(() -> titleBar.performClick());
        assertThat(userActionTester.getActions(), Matchers.hasItem("CustomTabs.OmniboxClicked"));
    }

    @Test
    @MediumTest
    public void omniboxInCCT_testInteractiveOmniboxOnEligibleCCTsWithAlternateHandler()
            throws Exception {
        var connection = Mockito.spy(CustomTabsConnection.getInstance());
        // Permit Omnibox for any upcoming intent(s).
        doReturn(true).when(connection).shouldEnableOmniboxForIntent(any());
        Consumer<Tab> mockConsumer = Mockito.mock(Consumer.class);
        // Provide an alternate tap handler on any upcoming intent(s).
        doReturn(mockConsumer).when(connection).getAlternateOmniboxTapHandler(any());
        CustomTabsConnection.setInstanceForTesting(connection);

        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        var tab = getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage);

        var titleBar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.title_url_container);
        Assert.assertTrue(titleBar.hasOnClickListeners());

        var url = mCustomTabActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(
                "Url bar should have an accessibility delegate.", url.getAccessibilityDelegate());

        UserActionTester userActionTester = new UserActionTester();
        ThreadUtils.runOnUiThreadBlocking(() -> titleBar.performClick());
        assertThat(userActionTester.getActions(), Matchers.hasItem("CustomTabs.OmniboxClicked"));
        verify(mockConsumer).accept(tab);
    }

    @Test
    @MediumTest
    public void omniboxInCCT_testNonInteractiveOmniboxWhenIntentNotEligible() {
        // By default, omnibox in CCT is not permitted and no stubbing is necessary.
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        var tab = getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage);

        var titleBar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.title_url_container);
        Assert.assertFalse(titleBar.hasOnClickListeners());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_NESTED_SECURITY_ICON)
    @MinAndroidSdkLevel(VERSION_CODES.R)
    public void titleAndUrlActionTest() throws ExecutionException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        var titleBar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.title_url_container);
        Assert.assertTrue(
                "Title bar should have a click listener.", titleBar.hasOnClickListeners());
        Assert.assertTrue(
                "Title bar should have a long press listener.", titleBar.hasOnLongClickListeners());

        var title = mCustomTabActivityTestRule.getActivity().findViewById(R.id.title_bar);
        Assert.assertNotNull(
                "Title should have an accessibility delegate.", title.getAccessibilityDelegate());

        var url = mCustomTabActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(
                "Url bar should have an accessibility delegate.", url.getAccessibilityDelegate());

        assertNull(
                "Page info hasn't been shown, so PageInfoController should be null.",
                PageInfoController.getLastPageInfoControllerForTesting());
        ThreadUtils.runOnUiThreadBlocking(() -> titleBar.performClick());
        assertNotNull(
                "Page info should have been shown.",
                PageInfoController.getLastPageInfoControllerForTesting());
    }

    private void rotateCustomTabActivity(CustomTabActivity activity, int orientation) {
        ActivityTestUtils.rotateActivityToOrientation(activity, orientation);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getResources().getConfiguration().orientation,
                            is(orientation));
                });
    }

    private void assertSideSheetOrBottomSheet(
            PartialCustomTabDisplayManager displayManager, int deviceWidthDp) {
        if (deviceWidthDp >= 600) {
            assertEquals(
                    "Incorrect strategy type",
                    PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                    displayManager.getActiveStrategyType());
        } else {
            assertEquals(
                    "Incorrect strategy type",
                    PartialCustomTabBaseStrategy.PartialCustomTabType.BOTTOM_SHEET,
                    displayManager.getActiveStrategyType());
        }
    }
}
