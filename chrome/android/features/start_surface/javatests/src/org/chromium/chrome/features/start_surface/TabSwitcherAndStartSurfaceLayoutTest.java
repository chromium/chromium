// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.os.Build.VERSION_CODES.O_MR1;
import static android.os.Build.VERSION_CODES.Q;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.components.embedder_support.util.UrlConstants.NTP_URL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.uiautomator.UiDevice;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorTestingRobot;
import org.chromium.chrome.browser.tasks.tab_management.TabSuggestionMessageService;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.UndoGroupSnackbarController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

// clang-format off
/** Tests for the {@link TabSwitcherAndStartSurfaceLayout} */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
// TODO(1315676): make this test suite works for testing both TabSwitcherAndStartSurfaceLayout and
// TabSwitcherLayout.
@DisableFeatures({ChromeFeatureList.START_SURFACE_REFACTOR})
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class TabSwitcherAndStartSurfaceLayoutTest {
    // clang-format on
    private static final String BASE_PARAMS = "force-fieldtrial-params="
            + "Study.Group:soft-cleanup-delay/0/cleanup-delay/0/skip-slow-zooming/false"
            + "/zooming-min-memory-mb/512";

    // Tests need animation on.
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(true);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    @Rule
    public HistogramTestRule mHistogramTester = new HistogramTestRule();

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;
    private TabSwitcherAndStartSurfaceLayout mTabSwitcherAndStartSurfaceLayout;
    private String mUrl;
    private int mRepeat;
    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private Callback<Bitmap> mBitmapListener =
            (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
    private TabSwitcher.TabListDelegate mTabListDelegate;

    @BeforeClass
    public static void beforeClass() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() throws ExecutionException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        // After setUp, Chrome is launched and has one NTP.
        mActivityTestRule.startMainActivityWithURL(NTP_URL);

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof TabSwitcherAndStartSurfaceLayout);
        mTabSwitcherAndStartSurfaceLayout = (TabSwitcherAndStartSurfaceLayout) layout;
        mUrl = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 1;

        mTabListDelegate = getTabListDelegateFromUIThread();
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());

        mActivityTestRule.getActivity().getTabContentManager().setCaptureMinRequestTimeForTesting(
                0);

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1300962")
    public void testRenderGrid_3WebTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "https://crbug.com/1300962")
    public void testRenderGrid_10WebTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "10_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "https://crbug.com/1139807")
    @DisableIf.Build(sdk_is_greater_than = O_MR1, message = "crbug.com/1077552")
    public void testRenderGrid_10WebTabs_InitialScroll() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        assertEquals(9, cta.getTabModelSelector().getCurrentModel().index());
        enterGTSWithThumbnailRetry();
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "10_web_tabs-select_last");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testSwitchTabModel_ScrollToSelectedTab() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        assertEquals(9, cta.getCurrentTabModel().index());
        createTabs(cta, true, 1);
        CriteriaHelper.pollUiThread(() -> cta.getCurrentTabModel().isIncognito());
        enterTabSwitcher(cta);
        switchTabModel(cta, false);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(tabSwitcherViewMatcher()).check((v, noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            assertTrue(v instanceof RecyclerView);
            LinearLayoutManager layoutManager =
                    (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
            assertEquals(9, layoutManager.findLastVisibleItemPosition());
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(message = "Flaky on emulators; see https://crbug.com/1324721 "
        + "and crbug.com/1077552", supported_abis_includes = "x86")
    public void testRenderGrid_Incognito() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito tabs and enter tab switcher.
        prepareTabs(1, 3, "about:blank");
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.tab_list_view));
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_incognito_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(message = "Flaky on emulators; see https://crbug.com/1313747",
        supported_abis_includes = "x86")
    public void testRenderGrid_3NativeTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito native tabs and enter tab switcher.
        // NTP in incognito mode is chosen for its consistency in look, and we don't have to mock
        // away the MV tiles, login promo, feed, etc.
        prepareTabs(1, 3, null);
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);
        // Espresso.pressBack();

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_incognito_ntps");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1, sdk_is_less_than = Q)
    public void testTabToGridFromLiveTab() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(0, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(0, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/991852 This test is flaky")
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    @DisableIf.Build(message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1, sdk_is_less_than = Q)
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(2000, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    @DisabledTest(message = "https://crbug.com/1207875")
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(0, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    @DisabledTest(message = "https://crbug.com/1272561")
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/1130830")
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);
    }

    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded, and assert no bitmap fetching occurred.
     *
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     */
    private void prepareTabs(int numTabs, int numIncognitoTabs, @Nullable String url) {
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, numTabs, numIncognitoTabs, url);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    private void testTabToGrid(String fromUrl) throws InterruptedException {
        mActivityTestRule.loadUrl(fromUrl);

        final int initCount = getCaptureCount();

        for (int i = 0; i < mRepeat; i++) {
            enterGTSWithThumbnailChecking();
            leaveGTSAndVerifyThumbnailsAreReleased();
        }
        checkFinalCaptureCount(false, initCount);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testGridToTabToCurrentNTP() throws InterruptedException {
        prepareTabs(1, 0, NTP_URL);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testGridToTabToOtherNTP() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisableIf.Build(message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1, sdk_is_less_than = Q)
    public void testGridToTabToCurrentLive() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    // From https://stackoverflow.com/a/21505193
    private static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic") || Build.FINGERPRINT.startsWith("unknown")
                || Build.MODEL.contains("google_sdk") || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                || (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic"))
                || "google_sdk".equals(Build.PRODUCT);
    }

    /**
     * Test that even if there are tabs with stuck pending thumbnail readback, it doesn't block
     * thumbnail readback for the current tab.
     */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    // clang-format off
    @DisableIf.Build(message = "Flaky on emulators; see https://crbug.com/1094492",
        supported_abis_includes = "x86")
    public void testGridToTabToCurrentLiveDetached() throws Exception {
        // clang-format on
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        // This works on emulators but not on real devices. See crbug.com/986047.
        if (!isEmulator()) return;

        for (int i = 0; i < 10; i++) {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            // Quickly create some tabs, navigate to web pages, and don't wait for thumbnail
            // capturing.
            mActivityTestRule.loadUrl(mUrl);
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, false, false);
            mActivityTestRule.loadUrl(mUrl);
            // Hopefully we are in a state where some pending readbacks are stuck because their tab
            // is not attached to the view.
            if (cta.getTabContentManager().getPendingReadbacksForTesting() > 0) {
                break;
            }

            // Restart Chrome.
            // Although we're destroying the activity, the Application will still live on since its
            // in the same process as this test.
            TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabModelSelector().closeAllTabs());
            TabUiTestHelper.finishActivity(cta);
            mActivityTestRule.startMainActivityOnBlankPage();
            assertEquals(1, mActivityTestRule.tabsCount(false));
        }
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertNotEquals(0, cta.getTabContentManager().getPendingReadbacksForTesting());
        assertEquals(1, cta.getCurrentTabModel().index());

        // The last tab should still get thumbnail even though readbacks for other tabs are stuck.
        enterTabSwitcher(cta);
        TabUiTestHelper.checkThumbnailsExist(cta.getTabModelSelector().getCurrentTab());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToCurrentLiveWithAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1313972")
    public void testGridToTabToOtherLive() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToOtherLiveWithAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1237623 test is flaky")
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, true);
    }

    private void testGridToTab(boolean switchToAnotherTab, boolean killBeforeSwitching)
            throws InterruptedException {
        final int initCount = getCaptureCount();

        for (int i = 0; i < mRepeat; i++) {
            enterGTSWithThumbnailChecking();

            final int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
            final int targetIndex = switchToAnotherTab ? 1 - index : index;
            Tab targetTab =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(targetIndex);
            if (killBeforeSwitching) {
                WebContentsUtils.simulateRendererKilled(targetTab.getWebContents());
            }

            if (switchToAnotherTab) {
                waitForCaptureRateControl();
            }
            int count = getCaptureCount();
            onView(tabSwitcherViewMatcher()).perform(actionOnItemAtPosition(targetIndex, click()));
            CriteriaHelper.pollUiThread(() -> {
                boolean doneHiding =
                        !mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                                LayoutType.TAB_SWITCHER);
                if (!doneHiding) {
                    // Before overview hiding animation is done, the tab index should not change.
                    Criteria.checkThat(mActivityTestRule.getActivity().getCurrentTabModel().index(),
                            Matchers.is(index));
                }
                return doneHiding;
            }, "Overview not hidden yet");
            int delta;
            if (switchToAnotherTab
                    && !UrlUtilities.isNTPUrl(mActivityTestRule.getActivity()
                                                      .getCurrentWebContents()
                                                      .getLastCommittedUrl())) {
                // Capture the original tab.
                delta = 1;
            } else {
                delta = 0;
            }
            checkCaptureCount(delta, count);
        }
        checkFinalCaptureCount(switchToAnotherTab, initCount);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRestoredTabsDontFetch() throws Exception {
        prepareTabs(2, 0, mUrl);
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Restart Chrome.
        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityOnBlankPage();
        assertEquals(3, mActivityTestRule.tabsCount(false));

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof TabSwitcherAndStartSurfaceLayout);
        mTabSwitcherAndStartSurfaceLayout = (TabSwitcherAndStartSurfaceLayout) layout;
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testInvisibleTabsDontFetch() throws InterruptedException {
        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    @DisabledTest(message = "crbug.com/1130830")
    public void testInvisibleTabsDontFetchWarm() throws InterruptedException {
        // Get the GTS in the warm state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    @DisabledTest(message = "crbug.com/1130830")
    public void testInvisibleTabsDontFetchSoft() throws InterruptedException {
        // Get the GTS in the soft cleaned up state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "http://crbug/1005865 - Test was previously flaky but only on bots."
            + "Was not locally reproducible. Disabling until verified that it's deflaked on bots.")
    public void testIncognitoEnterGts() throws InterruptedException {
        // clang-format on
        prepareTabs(1, 1, null);
        enterGTSWithThumbnailChecking();
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(1));

        onView(tabSwitcherViewMatcher()).perform(actionOnItemAtPosition(0, click()));
        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);

        enterGTSWithThumbnailChecking();
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(1));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    // clang-format off
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1063991",
        sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void testIncognitoToggle_tabCount() {
        // clang-format on
        mActivityTestRule.loadUrl(mUrl);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Prepare two incognito tabs and enter tab switcher.
        createTabs(cta, true, 2);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(cta, false);
            onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(1));

            switchTabModel(cta, true);
            onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        }
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "https://crbug.com/1233169")
    public void testIncognitoToggle_thumbnailFetchCount() throws InterruptedException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        int oldFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Prepare two incognito tabs and enter tab switcher.
        prepareTabs(1, 2, mUrl);
        enterGTSWithThumbnailChecking();

        int currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
        assertEquals(2, currentFetchCount - oldFetchCount);
        oldFetchCount = currentFetchCount;
        int oldHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(cta, false);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            int currentHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                    TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                    TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
            assertEquals(1, currentFetchCount - oldFetchCount);
            assertEquals(1, currentHistogramRecord - oldHistogramRecord);
            oldFetchCount = currentFetchCount;
            oldHistogramRecord = currentHistogramRecord;

            switchTabModel(cta, true);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            currentHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                    TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                    TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
            assertEquals(2, currentFetchCount - oldFetchCount);
            assertEquals(2, currentHistogramRecord - oldHistogramRecord);
            oldFetchCount = currentFetchCount;
            oldHistogramRecord = currentHistogramRecord;
        }
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                    ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testUrlUpdatedNotCrashing_ForUndoableClosedTab() throws Exception {
        // clang-format on
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(2, 0, null);
        enterGTSWithThumbnailChecking();

        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().closeTab(
                    tab, false, false, true);
        });
        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testUrlUpdatedNotCrashing_ForTabNotInCurrentModel() throws Exception {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(1, 1, null);
        enterGTSWithThumbnailChecking();

        Tab tab = cta.getTabModelSelector().getCurrentTab();
        switchTabModel(cta, false);

        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    private int getTabCountInCurrentTabModel() {
        return mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0/"
        + "thumbnail_aspect_ratio/1.0"})
    public void testTabSuggestionMessageCard_dismiss() throws InterruptedException {
        // clang-format on
        prepareTabs(3, 0, null);

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSuggestionMessageService.isSuggestionAvailableForTesting(). Instead, we can add a
        // dummy MessageObserver to track the availability of the suggestions.
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSwitcherCoordinator::hasAppendedMessagesForTesting. Instead, we can query the number
        // of items that the inner model of the TabSwitcher has.
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0"
        + "/thumbnail_aspect_ratio/1.0"})
    public void testTabSuggestionMessageCard_review() throws InterruptedException {
        // clang-format on
        prepareTabs(3, 0, null);

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();

        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        TabSelectionEditorTestingRobot tabSelectionEditorTestingRobot =
                new TabSelectionEditorTestingRobot();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        Espresso.pressBack();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @DisabledTest(message = "https://crbug.com/1230107, crbug.com/1130621")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0/"
        + "thumbnail_aspect_ratio/1.0"})
    public void testShowOnlyOneTabSuggestionMessageCard_withSoftCleanup()
            throws InterruptedException {
        // clang-format on
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0/"
        + "thumbnail_aspect_ratio/1.0"})
    @DisabledTest(message = "https://crbug.com/1198484, crbug.com/1130621")
    public void testShowOnlyOneTabSuggestionMessageCard_withHardCleanup()
            throws InterruptedException {
        // clang-format on
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0/"
        + "thumbnail_aspect_ratio/1.0"})
    @DisabledTest(message = "https://crbug.com/1311825")
    public void testTabSuggestionMessageCardDismissAfterTabClosing() throws InterruptedException {
        // clang-format on
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        closeFirstTabInTabSwitcher(mActivityTestRule.getActivity());

        CriteriaHelper.pollUiThread(
                () -> !TabSuggestionMessageService.isSuggestionAvailableForTesting());
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(2)));

        onView(tabSwitcherViewMatcher())
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabSuggestionMessageCardCount(
                        0));
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study",
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0/"
        + "thumbnail_aspect_ratio/1.0"})
    @DisabledTest(message = "https://crbug.com/1326533")
    public void testTabSuggestionMessageCard_orientation() throws InterruptedException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, null);
        View parentView = cta.getCompositorViewHolderForTesting();

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);

        // Force portrait mode since the device can be wrongly in landscape. See crbug/1063639.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() > parentView.getWidth());

        // Ensure the message card is visible so we can get its view holder.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.scrollToPosition(3))
                .check(MessageCardWidthAssertion.checkMessageItemSpanSize(3, 2));

        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() < parentView.getWidth());

        // Ensure the message card is visible so we can get its view holder.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.scrollToPosition(3))
                .check(MessageCardWidthAssertion.checkMessageItemSpanSize(3, 3));
    }

    private static class MessageCardWidthAssertion implements ViewAssertion {
        private int mIndex;
        private int mSpanCount;

        public static MessageCardWidthAssertion checkMessageItemSpanSize(int index, int spanCount) {
            return new MessageCardWidthAssertion(index, spanCount);
        }

        public MessageCardWidthAssertion(int index, int spanCount) {
            mIndex = index;
            mSpanCount = spanCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;
            float tabListPadding = TabUiThemeProvider.getTabGridCardMargin(view.getContext());
            float messageCardMargin =
                    TabUiThemeProvider.getMessageCardMarginDimension(view.getContext());

            assertTrue(view instanceof RecyclerView);
            RecyclerView recyclerView = (RecyclerView) view;
            GridLayoutManager layoutManager = (GridLayoutManager) recyclerView.getLayoutManager();
            assertEquals(mSpanCount, layoutManager.getSpanCount());

            RecyclerView.ViewHolder messageItemViewHolder =
                    recyclerView.findViewHolderForAdapterPosition(mIndex);
            assertNotNull(messageItemViewHolder);
            assertEquals(TabProperties.UiType.MESSAGE, messageItemViewHolder.getItemViewType());
            View messageItemView = messageItemViewHolder.itemView;

            // The message card item width should always be recyclerView width minus padding and
            // margin.
            assertEquals(recyclerView.getWidth() - 2 * tabListPadding - 2 * messageCardMargin,
                    (float) messageItemView.getWidth(), 1.0f);
        }
    }

    @Test
    @LargeTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_launch_polish/true"})
    @DisabledTest(message = "https://crbug.com/1122657")
    public void testThumbnailAspectRatio_default() {
        // clang-format on
        prepareTabs(2, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(
                        TabUtils.getTabThumbnailAspectRatio(mActivityTestRule.getActivity())));
    }

    @Test
    @LargeTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75/enable_launch_polish/true"})
    @DisabledTest(message = "https://crbug.com/1122657")
    public void testThumbnailAspectRatio_point75() {
        // clang-format on
        prepareTabs(2, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(0.75));
        leaveGTSAndVerifyThumbnailsAreReleased();

        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        mActivityTestRule.loadUrlInTab(
                NTP_URL, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(0.75));
    }

    @Test
    @LargeTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS
            + "/thumbnail_aspect_ratio/2.0/allow_to_refetch/true/enable_launch_polish/true"})
    @DisabledTest(message = "Flaky - https://crbug.com/1124041")
    public void testThumbnailAspectRatio_fromTwoToPoint75() throws Exception {
        // clang-format on
        prepareTabs(2, 0, "about:blank");
        // Select the first tab to ensure the second tab thumbnail is captured.
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            TabUiTestHelper.checkThumbnailsExist(currentTabModel.getTabAt(i));
        }
        leaveGTSAndVerifyThumbnailsAreReleased();

        simulateAspectRatioChangedToPoint75();
        verifyAllThumbnailHasAspectRatio(0.75);

        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testThumbnailFetchingResult_liveLayer() throws Exception {
        prepareTabs(1, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        // There might be an additional one from capturing thumbnail for the live layer.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAllBitmaps.size(), Matchers.greaterThanOrEqualTo(1)));

        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_ETC1));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG));
        assertEquals(1,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_NOTHING));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testThumbnailFetchingResult_jpeg() throws Exception {
        prepareTabs(1, 0, "about:blank");
        simulateJpegHasCachedWithDefaultAspectRatio();

        enterTabSwitcher(mActivityTestRule.getActivity());
        // There might be an additional one from capturing thumbnail for the live layer.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAllBitmaps.size(), Matchers.greaterThanOrEqualTo(1)));

        assertEquals(1,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_ETC1));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_NOTHING));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/2.0/allow_to_refetch/true"})
    @DisabledTest(message = "crbug.com/1315676#c20")
    public void testThumbnailFetchingResult_changingAspectRatio() throws Exception {
        prepareTabs(1, 0, "about:blank");
        // Simulate Jpeg has cached with default aspect ratio.
        simulateJpegHasCachedWithDefaultAspectRatio();
        enterTabSwitcher(mActivityTestRule.getActivity());
        // There might be an additional one from capturing thumbnail for the live layer.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAllBitmaps.size(), Matchers.greaterThanOrEqualTo(1)));

        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG));
        assertEquals(0,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_ETC1));
        assertEquals(1,
                mHistogramTester.getHistogramValueCount(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG));

        onViewWaiting(tabSwitcherViewMatcher())
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1297930")
    public void testRecycling_defaultAspectRatio() {
        prepareTabs(10, 0, mUrl);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(tabSwitcherViewMatcher()).perform(RecyclerViewActions.scrollToPosition(9));
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75"})
    @DisabledTest(message = "http://crbug/1092987")
    public void testRecycling_aspectRatioPoint75() {
        // clang-format on
        prepareTabs(10, 0, mUrl);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(tabSwitcherViewMatcher()).perform(RecyclerViewActions.scrollToPosition(9));
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75"})
    @DisabledTest(message = "https://crbug.com/1367310")
    public void testExpandTab_withAspectRatioPoint75() {
        // clang-format on
        prepareTabs(1, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/1.0"})
    @DisabledTest(message = "https://crbug.com/1359687")
    public void testRenderGrid_withAspectRatioOfOne() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, "about:blank");
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        // Make sure all tabs have thumbnail.
        enterGTSWithThumbnailRetry();
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "aspect_ratio_of_one");
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<study"})
    @DisabledTest(message = "https://crbug.com/1130212")
    public void testCloseTabViaCloseButton() throws Exception {
        // clang-format on
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(1, 0, null);
        enterGTSWithThumbnailChecking();

        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.content_view)),
                       withEffectiveVisibility(VISIBLE)))
                .perform(click());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<study"})
    @DisabledTest(message = "Flaky - https://crbug.com/1124041, crbug.com/1061178")
    public void testSwipeToDismiss_GTS() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 3 tabs and merge the first two tabs into one group.
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 2);
        verifyTabModelTabCount(cta, 3, 0);

        // Swipe to dismiss a single tab card.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(false)));
        verifyTabSwitcherCardCount(cta, 1);
        verifyTabModelTabCount(cta, 2, 0);

        // Swipe to dismiss a tab group card.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        0, getSwipeToDismissAction(true)));
        verifyTabSwitcherCardCount(cta, 0);
        verifyTabModelTabCount(cta, 0, 0);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_launch_polish/true"})
    public void testCloseButtonDescription() {
        String expectedDescription = "Close New tab tab";
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        // Test single tab.
        onView(allOf(withParent(withId(R.id.content_view)), withId(R.id.action_button),
                       withEffectiveVisibility(VISIBLE)))
                .check(ViewContentDescription.havingDescription(expectedDescription));

        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 1);

        // Test group tab.
        expectedDescription = "Close tab group with 2 tabs";
        onView(allOf(withParent(withId(R.id.content_view)), withId(R.id.action_button),
                       withEffectiveVisibility(VISIBLE)))
                .check(ViewContentDescription.havingDescription(expectedDescription));
    }

    private static class ViewContentDescription implements ViewAssertion {
        private String mExpectedDescription;

        public static ViewContentDescription havingDescription(String description) {
            return new ViewContentDescription(description);
        }

        public ViewContentDescription(String description) {
            mExpectedDescription = description;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            assertEquals(mExpectedDescription, view.getContentDescription());
        }
    }

    private static class TabCountAssertion implements ViewAssertion {
        private int mExpectedCount;

        public static TabCountAssertion havingTabCount(int tabCount) {
            return new TabCountAssertion(tabCount);
        }

        public TabCountAssertion(int expectedCount) {
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView.Adapter adapter = ((RecyclerView) view).getAdapter();
            assertEquals(mExpectedCount, adapter.getItemCount());
        }
    }

    private static class ThumbnailAspectRatioAssertion implements ViewAssertion {
        private double mExpectedRatio;

        public static ThumbnailAspectRatioAssertion havingAspectRatio(double ratio) {
            return new ThumbnailAspectRatioAssertion(ratio);
        }

        private ThumbnailAspectRatioAssertion(double expectedRatio) {
            mExpectedRatio = expectedRatio;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = (RecyclerView) view;

            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            boolean hasAtLeastOneValidViewHolder = false;
            for (int i = 0; i < adapter.getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder != null) {
                    hasAtLeastOneValidViewHolder = true;
                    ViewLookupCachingFrameLayout tabView =
                            (ViewLookupCachingFrameLayout) viewHolder.itemView;
                    TabGridThumbnailView thumbnail =
                            (TabGridThumbnailView) tabView.fastFindViewById(R.id.tab_thumbnail);
                    double thumbnailViewRatio = thumbnail.getWidth() * 1.0 / thumbnail.getHeight();

                    assertTrue("Actual ratio: " + thumbnailViewRatio
                                    + "; Expected ratio: " + mExpectedRatio,
                            Math.abs(thumbnailViewRatio - mExpectedRatio)
                                    <= TabContentManager.ASPECT_RATIO_PRECISION);
                }
            }
            assertTrue("should have at least one valid ViewHolder", hasAtLeastOneValidViewHolder);
        }
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1096997")
    public void testTabGroupManualSelection() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(3));

        enterTabGroupManualSelection(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarActionButton();

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testTabGroupManualSelection_DisabledForSingleTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Group option should be disabled when there is only one single tab.
        // We are using UiDevice to finish the click here since there seems to be more than one menu
        // button in the view hierarchy, and we couldn't locate it using espresso approach. Also,
        // performClick() won't work as the logic that handles menu button lies in onTouchListener.
        View menuButton =
                cta.findViewById(R.id.tab_switcher_toolbar).findViewById(R.id.menu_button);
        int[] location = new int[2];
        menuButton.getLocationOnScreen(location);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .click(location[0], location[1]);
        // Even if we can tell the group option is disabled by looking at the device when the test
        // is running, the espresso view matcher says it is enabled and all view parameters are the
        // same with the one when we have two tabs. Thus, we check the item's click response here to
        // tell if it is enabled.
        onViewWaiting(withText("Group tabs"))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        robot.resultRobot.verifyTabSelectionEditorIsHidden();

        // Group option should be enabled when there is more than one single tab.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .click(location[0], location[1]);
        onViewWaiting(withText("Group tabs"))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        robot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testTabGroupManualSelection_SystemBackDismiss() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        enterTabGroupManualSelection(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Pressing system back should dismiss the selection editor.
        Espresso.pressBack();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2,
                     ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    public void testTabSelectionEditorV2_SystemBackDismiss() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        enterTabSelectionEditorV2(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Pressing system back should dismiss the selection editor.
        Espresso.pressBack();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study"})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0" +
            "/thumbnail_aspect_ratio/1.0"})
    public void testTabGroupManualSelection_AfterReviewTabSuggestion() throws InterruptedException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 3);

        // Review closing tab suggestion.
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        // Entering GTS with thumbnail checking here is trying to reduce flakiness that is caused by
        // the TabContextObserver. TabContextObserver listens to
        // TabObserver#didFirstVisuallyNonEmptyPaint and invalidates the suggestion. Do the
        // thumbnail checking here is to ensure the suggestion is valid when entering tab switcher.
        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonWithResourceId(
                        R.string.tab_suggestion_close_tab_action_button);

        robot.actionRobot.clickToolbarActionButton();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityTestRule.getActivity().getCurrentTabModel().getCount(),
                    Matchers.is(0));
        });

        // Show Manual Selection Mode.
        createTabs(cta, false, 3);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        enterTabGroupManualSelection(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group);

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarActionButton();

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "crbug.com/1187320 This doesn't work with FeedV2 and crbug.com/1096295")
    public void testActivityCanBeGarbageCollectedAfterFinished() throws Exception {
        prepareTabs(1, 0, "about:blank");

        WeakReference<ChromeTabbedActivity> activityRef =
                new WeakReference<>(mActivityTestRule.getActivity());

        ChromeTabbedActivity activity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mTabSwitcherAndStartSurfaceLayout = null;
        mTabListDelegate = null;
        mActivityTestRule.setActivity(activity);

        // A longer timeout is needed. Achieve that by using the CriteriaHelper.pollUiThread.
        CriteriaHelper.pollUiThread(
                () -> GarbageCollectionTestUtils.canBeGarbageCollected(activityRef));
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testTabGroupNotFormDuringRestore() throws Exception {
        // clang-format on
        // Finish the activity and disable the ChromeFeatureList.TAB_GROUPS_ANDROID flag.
        finishActivity(mActivityTestRule.getActivity());
        ChromeFeatureList.sTabGroupsAndroid.setForTesting(false);

        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(mUrl);
        Tab parentTab = cta.getTabModelSelector().getCurrentTab();

        // Create a tab whose parent tab is parentTab.
        TabCreator tabCreator =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> cta.getTabCreator(false));
        LoadUrlParams loadUrlParams = new LoadUrlParams(mUrl);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tabCreator.createNewTab(
                                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, parentTab));
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Restart activity with tab group enabled, and the tabs should remain as single tabs.
        finishActivity(cta);
        ChromeFeatureList.sTabGroupsAndroid.setForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity ctaRestarted = mActivityTestRule.getActivity();
        assertTrue(ctaRestarted.getTabModelSelector()
                           .getTabModelFilterProvider()
                           .getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        enterTabSwitcher(ctaRestarted);
        verifyTabSwitcherCardCount(ctaRestarted, 3);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void verifyTabGroupStateAfterReparenting() throws Exception {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        mActivityTestRule.loadUrl(mUrl);
        Tab parentTab = cta.getTabModelSelector().getCurrentTab();

        // Create a tab whose parent tab is parentTab.
        TabCreator tabCreator =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> cta.getTabCreator(false));
        LoadUrlParams loadUrlParams = new LoadUrlParams(mUrl);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tabCreator.createNewTab(
                                loadUrlParams, TabLaunchType.FROM_LONGPRESS_BACKGROUND, parentTab));
        Tab childTab = cta.getTabModelSelector().getCurrentModel().getTabAt(1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getCurrentTabModelFilter();
        TestThreadUtils.runOnUiThreadBlocking(() -> filter.moveTabOutOfGroup(childTab.getId()));
        verifyTabSwitcherCardCount(cta, 2);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(true));
        final ChromeTabbedActivity ctaNightMode = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        assertTrue(ColorUtils.inNightMode(ctaNightMode));
        CriteriaHelper.pollUiThread(ctaNightMode.getTabModelSelector()::isTabStateInitialized);
        enterTabSwitcher(ctaNightMode);
        verifyTabSwitcherCardCount(ctaNightMode, 2);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testUndoClosure_AccessibilityMode() throws Exception {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);

        // When grid tab switcher is enabled for accessibility mode, tab closure should still show
        // undo snack bar.
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());
        closeFirstTabInTabSwitcher(cta);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 2);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID, ChromeFeatureList.INSTANT_START,
            ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    // TODO(crbug.com/1112557): Remove this test when critical tests in StartSurfaceLayoutTest are
    // running with InstantStart on.
    public void testSetup_WithInstantStart() throws Exception {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify TabModelObserver is correctly setup by checking if tab switcher changes with tab
        // closure.
        closeFirstTabInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 0);

        // Verify TabGroupModelFilter is correctly setup by checking if tab switcher changes with
        // tab grouping.
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testUndoGroupClosureInTabSwitcher() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        assertNotNull(snackbarManager.getCurrentSnackbarForTesting());

        // Verify close this tab group and undo in tab switcher.
        closeFirstTabInTabSwitcher(cta);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @MediumTest
    public void testLongPressTab_entryInTabSwitcher_verifyNoSelectionOccurs() {
        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabSelectionEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabSelectionEditorTestingRobot mSelectionEditorRobot = new TabSelectionEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Verify no selection action occurred to switch the selected tab in the tab model
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(1));

        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @MediumTest
    public void testLongPressTabGroup_entryInTabSwitcher() {
        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // LongPress entry to TabSelectionEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabSelectionEditorTestingRobot mSelectionEditorRobot = new TabSelectionEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @MediumTest
    public void testLongPressTab_verifyPostLongPressClickNoSelectionEditor() {
        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabSelectionEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabSelectionEditorTestingRobot mSelectionEditorRobot = new TabSelectionEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0);
        Espresso.pressBack();

        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        // Check the selected tab in the tab model switches from the second tab to the first to
        // verify clicking the tab worked.
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(0));

        TabUiFeatureUtilities.ENABLE_TAB_SELECTION_EDITOR_V2_LONGPRESS_ENTRY.setForTesting(false);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_TabToTab() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals("2", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_TabToGroupAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 2);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);
        assertEquals("2", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals("3", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_GroupToGroupNonAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        // Merge last two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(3), normalTabModel.getTabAt(4)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 4);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);
        assertEquals("2", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());

        // Merge first two tabs into a group.
        List<Tab> tabGroup2 = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup2);
        verifyTabSwitcherCardCount(cta, 3);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);
        assertEquals("2", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());

        // Merge the two tab groups into a group.
        List<Tab> tabGroup3 = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(3)));
        createTabGroup(cta, false, tabGroup3);
        assertTrue(snackbarManager.getCurrentSnackbarForTesting().getController()
                           instanceof UndoGroupSnackbarController);

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals("4", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
    }

    private TabSwitcher.TabListDelegate getTabListDelegateFromUIThread() {
        AtomicReference<TabSwitcher.TabListDelegate> tabListDelegate = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tabListDelegate.set(
                                mTabSwitcherAndStartSurfaceLayout.getStartSurfaceForTesting()
                                        .getGridTabListDelegate()));
        return tabListDelegate.get();
    }

    private void enterTabGroupManualSelection(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_group_tabs);
    }

    private void enterTabSelectionEditorV2(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_select_tabs);
    }

    /**
     * TODO(wychen): move some of the callers to {@link TabUiTestHelper#enterTabSwitcher}.
     */
    private void enterGTSWithThumbnailChecking() throws InterruptedException {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            mActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());
        }

        int count = getCaptureCount();
        waitForCaptureRateControl();
        // TODO(wychen): use TabUiTestHelper.enterTabSwitcher() instead.
        //  Might increase flakiness though. See crbug.com/1024742.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);

        // Make sure the fading animation is done.
        int delta;
        if (UrlUtilities.isNTPUrl(mActivityTestRule.getActivity()
                                          .getCurrentWebContents()
                                          .getLastCommittedUrl())) {
            // NTP is not invalidated, so no new captures.
            delta = 0;
        } else {
            // The final capture at StartSurfaceLayout#finishedShowing time.
            delta = 1;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                // The faster capturing without writing back to cache.
                delta += 1;
            }
        }
        checkCaptureCount(delta, count);
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
    }

    /**
     * Like {@link TabUiTestHelper#enterTabSwitcher}, but make sure all tabs have thumbnail.
     */
    private void enterGTSWithThumbnailRetry() {
        enterTabSwitcher(mActivityTestRule.getActivity());
        try {
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        } catch (AssertionError ae) {
            // If the last thumbnail is missing, try without animation.
            Espresso.pressBack();
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> mActivityTestRule.getActivity().getLayoutManager().showLayout(
                                    LayoutType.TAB_SWITCHER, false));
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        }
    }

    /**
     * If thumbnail checking is not needed, use {@link TabUiTestHelper#leaveTabSwitcher} instead.
     */
    private void leaveGTSAndVerifyThumbnailsAreReleased() {
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().isLayoutVisible(
                LayoutType.TAB_SWITCHER));

        StartSurface startSurface = mTabSwitcherAndStartSurfaceLayout.getStartSurfaceForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> { startSurface.onBackPressed(); });
        // TODO(wychen): using default timeout or even converting to
        //  OverviewModeBehaviorWatcher shouldn't increase flakiness.
        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        assertThumbnailsAreReleased();
    }

    private void checkFinalCaptureCount(boolean switchToAnotherTab, int initCount) {
        int expected;
        if (UrlUtilities.isNTPUrl(mActivityTestRule.getActivity()
                                          .getCurrentWebContents()
                                          .getLastCommittedUrl())) {
            expected = 0;
        } else {
            expected = mRepeat;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                expected += mRepeat;
            }
            if (switchToAnotherTab) {
                expected += mRepeat;
            }
        }
        checkCaptureCount(expected, initCount);
    }

    private void checkCaptureCount(int expectedDelta, int initCount) {
        // TODO(wychen): With animation, the 2nd capture might be skipped if the 1st takes too long.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(getCaptureCount() - initCount, Matchers.is(expectedDelta));
        });
    }

    private int getCaptureCount() {
        // TODO(crbug/1110961): Find a replacement for depending on Compositing.CopyFromSurfaceTime.
        return RecordHistogram.getHistogramTotalCountForTesting("Compositing.CopyFromSurfaceTime");
    }

    private void waitForCaptureRateControl() throws InterruptedException {
        // Needs to wait for at least |kCaptureMinRequestTimeMs| in order to capture another one.
        // TODO(wychen): find out why waiting is still needed after setting
        //               |kCaptureMinRequestTimeMs| to 0.
        Thread.sleep(2000);
    }

    private void assertThumbnailsAreReleased() {
        // Could not directly assert canAllBeGarbageCollected() because objects can be in Cleaner.
        CriteriaHelper.pollUiThread(() -> canAllBeGarbageCollected(mAllBitmaps));
        mAllBitmaps.clear();
    }

    private boolean canAllBeGarbageCollected(List<WeakReference<Bitmap>> bitmaps) {
        for (WeakReference<Bitmap> bitmap : bitmaps) {
            if (!GarbageCollectionTestUtils.canBeGarbageCollected(bitmap)) {
                return false;
            }
        }
        return true;
    }

    private void simulateJpegHasCachedWithDefaultAspectRatio() throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        int jpegWidth = 125;
        int jpegHeight = (int) (jpegWidth * 1.0
                / TabUtils.getTabThumbnailAspectRatio(mActivityTestRule.getActivity()));
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = Bitmap.createBitmap(jpegWidth, jpegHeight, Config.ARGB_8888);
            encodeJpeg(tab, bitmap);
        }
    }

    private void simulateAspectRatioChangedToPoint75() throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = TabContentManager.getJpegForTab(tab.getId(), null);
            bitmap = Bitmap.createScaledBitmap(
                    bitmap, bitmap.getWidth(), (int) (bitmap.getWidth() * 1.0 / 0.75), false);
            encodeJpeg(tab, bitmap);
        }
    }

    private void encodeJpeg(Tab tab, Bitmap bitmap) throws IOException {
        FileOutputStream outputStream =
                new FileOutputStream(TabContentManager.getTabThumbnailFileJpeg(tab.getId()));
        bitmap.compress(Bitmap.CompressFormat.JPEG, 50, outputStream);
        outputStream.close();
    }

    private void verifyAllThumbnailHasAspectRatio(double ratio) {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = TabContentManager.getJpegForTab(tab.getId(), null);
            double bitmapRatio = bitmap.getWidth() * 1.0 / bitmap.getHeight();
            assertTrue("Actual ratio: " + bitmapRatio + "; Expected ratio: " + ratio,
                    Math.abs(bitmapRatio - ratio) <= TabContentManager.ASPECT_RATIO_PRECISION);
        }
    }

    private void verifyOnlyOneTabSuggestionMessageCardIsShowing() throws InterruptedException {
        String suggestionMessageTemplate = mActivityTestRule.getActivity().getString(
                org.chromium.chrome.tab_ui.R.string.tab_suggestion_close_stale_message);
        String suggestionMessage =
                String.format(Locale.getDefault(), suggestionMessageTemplate, "3");
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
        leaveGTSAndVerifyThumbnailsAreReleased();

        // With soft or hard clean up depends on the soft-cleanup-delay and cleanup-delay params.
        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        // This will fail with error "matched multiple views" when there is more than one suggestion
        // message card.
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
    }

    private Matcher<View> tabSwitcherViewMatcher() {
        return allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(
                             mActivityTestRule.getActivity()))),
                withId(R.id.tab_list_view));
    }
}
