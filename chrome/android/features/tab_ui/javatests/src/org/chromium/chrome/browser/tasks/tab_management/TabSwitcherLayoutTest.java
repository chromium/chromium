// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.os.Build.VERSION_CODES.O_MR1;
import static android.os.Build.VERSION_CODES.Q;

import static androidx.test.espresso.Espresso.closeSoftKeyboard;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.components.embedder_support.util.UrlConstants.NTP_URL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;
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

/** Tests for the {@link TabSwitcherLayout}. */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@EnableFeatures({ChromeFeatureList.DEFER_TAB_SWITCHER_LAYOUT_CREATION})
@DisableFeatures({ChromeFeatureList.ANDROID_HUB, ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
@Restriction({
    UiRestriction.RESTRICTION_TYPE_PHONE,
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE
})
public class TabSwitcherLayoutTest {
    private static final String BASE_PARAMS =
            "force-fieldtrial-params="
                    + "Study.Group:skip-slow-zooming/false/zooming-min-memory-mb/512";

    private static final String TEST_URL = "/chrome/test/data/android/google.html";

    private static final int INVALID_COLOR_ID = -1;

    // Tests need animation on.
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(true);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;

    private TabSwitcherLayout mTabSwitcherLayout;
    private String mUrl;
    private int mRepeat;
    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private Callback<Bitmap> mBitmapListener =
            (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
    private TabSwitcher.TabListDelegate mTabListDelegate;
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() throws ExecutionException {
        mTestServer = mActivityTestRule.getTestServer();

        // After setUp, Chrome is launched and has one NTP.
        mActivityTestRule.startMainActivityWithURL(NTP_URL);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Layout layout = TabUiTestHelper.getTabSwitcherLayoutAndVerify(cta);
        mTabSwitcherLayout = (TabSwitcherLayout) layout;
        mUrl = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 1;

        mTabListDelegate = getTabListDelegateFromUIThread();
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());

        mActivityTestRule
                .getActivity()
                .getTabContentManager()
                .setCaptureMinRequestTimeForTesting(0);

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        mActivityTestRule.getActivity()::getModalDialogManager);

        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null));
        dismissAllModalDialogs();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/1469431")
    public void testRenderGrid_3WebTabs() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_recycler_view), "3_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "crbug.com/1473722",
            supported_abis_includes = "x86",
            sdk_is_less_than = VERSION_CODES.P)
    public void testRenderGrid_3WebTabs_ThumbnailCacheRefactor() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, mTestServer.getURL(TEST_URL));
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterGTSWithThumbnailRetry();
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_recycler_view),
                "3_web_tabs_thumbnail_cache_refactor");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableAnimationsTestRule.EnsureAnimationsOn
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testRenderGrid_10WebTabs() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_recycler_view), "10_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testRenderGrid_10WebTabs_InitialScroll() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        assertEquals(9, cta.getTabModelSelector().getCurrentModel().index());
        enterGTSWithThumbnailRetry();
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_recycler_view), "10_web_tabs-select_last");
    }

    // TODO(crbug/324919909): Delete this test once Hub is launched. It is migrated to
    // TabSwitcherPanePublicTransitTest.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testSwitchTabModel_ScrollToSelectedTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        assertEquals(9, cta.getCurrentTabModel().index());
        createTabs(cta, true, 1);
        CriteriaHelper.pollUiThread(() -> cta.getCurrentTabModel().isIncognito());
        enterTabSwitcher(cta);
        switchTabModel(cta, false);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(tabSwitcherViewMatcher())
                .check(
                        (v, noMatchException) -> {
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
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message =
                    "Flaky on emulators; see https://crbug.com/1324721 " + "and crbug.com/1077552",
            supported_abis_includes = "x86")
    public void testRenderGrid_Incognito() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito tabs and enter tab switcher.
        prepareTabs(1, 3, "about:blank");
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.tab_list_recycler_view));
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_recycler_view), "3_incognito_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on emulators; see https://crbug.com/1313747",
            supported_abis_includes = "x86")
    public void testRenderGrid_3NativeTabs() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito native tabs and enter tab switcher.
        // NTP in incognito mode is chosen for its consistency in look, and we don't have to mock
        // away the MV tiles, login promo, feed, etc.
        prepareTabs(1, 3, null);
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_recycler_view), "3_incognito_ntps");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderGrid_1TabGroup_ColorIcon() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Hardcode TabGroupColorId.GREY as the tab group color for render testing purposes.
        Tab tab = cta.getTabModelSelector().getCurrentTab();
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> filter.setTabGroupColor(tab.getRootId(), TabGroupColorId.GREY));
        // Leave and re enter GTS to refetch the favicon.
        leaveTabSwitcher(cta);
        enterTabSwitcher(cta);
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_recycler_view),
                "1_tab_group_GTS_card_item_color_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "Flaky on emulators; see https://crbug.com/1313747",
            supported_abis_includes = "x86")
    public void testRenderGrid_3NativeTabs_ThumbnailCacheRefactor() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito native tabs and enter tab switcher.
        // NTP in incognito mode is chosen for its consistency in look, and we don't have to mock
        // away the MV tiles, login promo, feed, etc.
        prepareTabs(1, 3, null);
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_recycler_view),
                "3_incognito_ntps_thumbnail_cache_refactor");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(
            message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1,
            sdk_is_less_than = Q)
    public void testTabToGridFromLiveTab() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/991852 This test is flaky")
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisableIf.Build(
            message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1,
            sdk_is_less_than = Q)
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1207875")
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1272561")
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
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

        for (int i = 0; i < mRepeat; i++) {
            enterGTSWithThumbnailChecking();
            leaveGTSAndVerifyThumbnailsAreReleased();
        }
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testGridToTabToCurrentNtp() throws InterruptedException {
        prepareTabs(1, 0, NTP_URL);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testGridToTabToOtherNtp() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisableIf.Build(
            message = "https://crbug.com/1365708",
            supported_abis_includes = "x86",
            sdk_is_greater_than = O_MR1,
            sdk_is_less_than = Q)
    public void testGridToTabToCurrentLive() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    // From https://stackoverflow.com/a/21505193
    private static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic")
                || Build.FINGERPRINT.startsWith("unknown")
                || Build.MODEL.contains("google_sdk")
                || Build.MODEL.contains("Emulator")
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
    @DisableIf.Build(
            message = "Flaky on emulators; see https://crbug.com/1094492",
            supported_abis_includes = "x86")
    public void testGridToTabToCurrentLiveDetached() throws Exception {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
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
            if (cta.getTabContentManager().getInFlightCapturesForTesting() > 0) {
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
        assertNotEquals(0, cta.getTabContentManager().getInFlightCapturesForTesting());
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
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1313972")
    public void testGridToTabToOtherLive() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToOtherLiveWithAnimation() throws InterruptedException {
        assertTrue(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1237623 test is flaky")
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        assertFalse(
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivityTestRule.getActivity()));
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, true);
    }

    private void testGridToTab(boolean switchToAnotherTab, boolean killBeforeSwitching)
            throws InterruptedException {
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
            onView(tabSwitcherViewMatcher()).perform(actionOnItemAtPosition(targetIndex, click()));
            CriteriaHelper.pollUiThread(
                    () -> {
                        boolean doneHiding =
                                !mActivityTestRule
                                        .getActivity()
                                        .getLayoutManager()
                                        .isLayoutVisible(LayoutType.TAB_SWITCHER);
                        if (!doneHiding) {
                            // Before overview hiding animation is done, the tab index should not
                            // change.
                            Criteria.checkThat(
                                    mActivityTestRule.getActivity().getCurrentTabModel().index(),
                                    Matchers.is(index));
                        }
                        return doneHiding;
                    },
                    "Overview not hidden yet");
        }
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

        TabUiTestHelper.getTabSwitcherLayoutAndVerify(mActivityTestRule.getActivity());
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
            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/1130830")
    public void testInvisibleTabsDontFetchWarm() throws InterruptedException {
        // Get the GTS in the warm state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "crbug.com/1130830")
    public void testInvisibleTabsDontFetchSoft() throws InterruptedException {
        // Get the GTS in the soft cleaned up state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(
            message =
                    "http://crbug/1005865 - Test was previously flaky but only on bots.Was not"
                        + " locally reproducible. Disabling until verified that it's deflaked on"
                        + " bots.")
    public void testIncognitoEnterGts() throws InterruptedException {
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
    public void testIncognitoToggle_tabCount() throws InterruptedException {
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
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "https://crbug.com/1233169")
    public void testIncognitoToggle_thumbnailFetchCount() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        int oldFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Prepare two incognito tabs and enter tab switcher.
        prepareTabs(1, 2, mUrl);
        enterGTSWithThumbnailChecking();

        int currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
        assertEquals(2, currentFetchCount - oldFetchCount);
        oldFetchCount = currentFetchCount;
        int oldHistogramRecord =
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG);

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(cta, false);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            int currentHistogramRecord =
                    RecordHistogram.getHistogramValueCountForTesting(
                            TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                            TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
            assertEquals(1, currentFetchCount - oldFetchCount);
            assertEquals(1, currentHistogramRecord - oldHistogramRecord);
            oldFetchCount = currentFetchCount;
            oldHistogramRecord = currentHistogramRecord;

            switchTabModel(cta, true);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            currentHistogramRecord =
                    RecordHistogram.getHistogramValueCountForTesting(
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
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testUrlUpdatedNotCrashing_ForUndoableClosedTab() throws Exception {
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(2, 0, null);
        enterGTSWithThumbnailChecking();

        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .closeTab(tab, false, false, true);
                });
        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testUrlUpdatedNotCrashing_ForTabNotInCurrentModel() throws Exception {
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
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1458026 for RefactorDisabled")
    public void testTabSuggestionMessageCard_dismiss() throws InterruptedException {
        prepareTabs(3, 0, null);

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSuggestionMessageService.isSuggestionAvailableForTesting(). Instead, we can add a
        // mock/fake MessageObserver to track the availability of the suggestions.
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSwitcherMessageManager::hasAppendedMessagesForTesting. Instead, we can query the
        // number
        // of items that the inner model of the TabSwitcher has.
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        ViewUtils.onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1447282 for refactor disabled case.")
    public void testTabSuggestionMessageCard_review() throws InterruptedException {
        prepareTabs(3, 0, null);

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();

        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        ViewUtils.onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.action_button),
                                withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        TabListEditorTestingRobot tabListEditorTestingRobot = new TabListEditorTestingRobot();
        tabListEditorTestingRobot.resultRobot.verifyTabListEditorIsVisible();

        Espresso.pressBack();
        tabListEditorTestingRobot.resultRobot.verifyTabListEditorIsHidden();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @DisabledTest(message = "https://crbug.com/1230107, crbug.com/1130621")
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    public void testShowOnlyOneTabSuggestionMessageCard_withSoftCleanup()
            throws InterruptedException {
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1198484, crbug.com/1130621")
    public void testShowOnlyOneTabSuggestionMessageCard_withHardCleanup()
            throws InterruptedException {
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1311825")
    public void testTabSuggestionMessageCardDismissAfterTabClosing() throws InterruptedException {
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        ViewUtils.onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        closeFirstTabInTabSwitcher(mActivityTestRule.getActivity());

        CriteriaHelper.pollUiThread(
                () -> !TabSuggestionMessageService.isSuggestionAvailableForTesting());
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(2)));

        onView(tabSwitcherViewMatcher())
                .check(
                        TabUiTestHelper.ChildrenCountAssertion.havingTabSuggestionMessageCardCount(
                                0));
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({
        ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"
    })
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1326533")
    public void testTabSuggestionMessageCard_orientation() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, null);
        View parentView = cta.getCompositorViewHolderForTesting();

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);

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
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
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
            assertEquals(
                    recyclerView.getWidth() - 2 * tabListPadding - 2 * messageCardMargin,
                    (float) messageItemView.getWidth(),
                    1.0f);
        }
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "https://crbug.com/1122657")
    public void testThumbnailAspectRatio_default() {
        prepareTabs(2, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(
                        ThumbnailAspectRatioAssertion.havingAspectRatio(
                                TabUtils.getTabThumbnailAspectRatio(
                                        mActivityTestRule.getActivity(),
                                        mActivityTestRule
                                                .getActivity()
                                                .getBrowserControlsManager())));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({
        BASE_PARAMS,
        // TODO(crbug.com/1491942): This fails with the field trial testing config.
        "disable-field-trial-config"
    })
    public void testThumbnailFetchingResult_liveLayer() {
        // May be called when setting both grid card size and thumbnail fetcher.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                                TabContentManager.ThumbnailFetchingResult.GOT_NOTHING)
                        .allowExtraRecords(TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT)
                        .build();

        prepareTabs(1, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        // There might be an additional one from capturing thumbnail for the live layer.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAllBitmaps.size(), Matchers.greaterThanOrEqualTo(1)));

        histograms.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testThumbnailFetchingResult_jpeg() throws Exception {
        // May be called when setting both grid card size and thumbnail fetcher.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                                TabContentManager.ThumbnailFetchingResult.GOT_JPEG)
                        .allowExtraRecords(TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT)
                        .build();

        prepareTabs(1, 0, "about:blank");
        simulateJpegHasCachedWithDefaultAspectRatio();

        enterTabSwitcher(mActivityTestRule.getActivity());
        // There might be an additional one from capturing thumbnail for the live layer.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAllBitmaps.size(), Matchers.greaterThanOrEqualTo(1)));

        histograms.assertExpected();
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
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testExpandTab() throws InterruptedException {
        prepareTabs(1, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testExpandTab_ThumbnailCacheRefactor() throws InterruptedException {
        prepareTabs(1, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void testCloseTabViaCloseButton() throws Exception {
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(1, 0, null);
        enterGTSWithThumbnailChecking();

        onView(
                        allOf(
                                withId(R.id.action_button),
                                withParent(withId(R.id.content_view)),
                                withEffectiveVisibility(VISIBLE)))
                .perform(click());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "Flaky - https://crbug.com/1124041, crbug.com/1061178")
    public void testSwipeToDismiss_GTS() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 3 tabs and merge the first two tabs into one group.
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 2);
        verifyTabModelTabCount(cta, 3, 0);

        // Swipe to dismiss a single tab card.
        onView(tabSwitcherViewMatcher())
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                1, getSwipeToDismissAction(false)));
        verifyTabSwitcherCardCount(cta, 1);
        verifyTabModelTabCount(cta, 2, 0);

        // Swipe to dismiss a tab group card.
        onView(tabSwitcherViewMatcher())
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                0, getSwipeToDismissAction(true)));
        verifyTabSwitcherCardCount(cta, 0);
        verifyTabModelTabCount(cta, 0, 0);
    }

    @Test
    @MediumTest
    public void testCloseButtonDescription() {
        String expectedDescription = "Close New tab tab";
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        // Test single tab.
        onView(
                        allOf(
                                withParent(withId(R.id.content_view)),
                                withId(R.id.action_button),
                                withEffectiveVisibility(VISIBLE)))
                .check(ViewContentDescription.havingDescription(expectedDescription));

        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 1);

        // Test group tab.
        expectedDescription = "Close tab group with 2 tabs.";
        onView(
                        allOf(
                                withParent(withId(R.id.content_view)),
                                withId(R.id.action_button),
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
                    TabThumbnailView thumbnail =
                            (TabThumbnailView) tabView.fastFindViewById(R.id.tab_thumbnail);

                    double thumbnailViewRatio = thumbnail.getWidth() * 1.0 / thumbnail.getHeight();
                    int pixelDelta =
                            Math.abs(
                                    (int) Math.round(thumbnail.getHeight() * mExpectedRatio)
                                            - thumbnail.getWidth());
                    assertTrue(
                            "Actual ratio: "
                                    + thumbnailViewRatio
                                    + "; Expected ratio: "
                                    + mExpectedRatio
                                    + "; Pixel delta: "
                                    + pixelDelta,
                            pixelDelta
                                    <= thumbnail.getWidth()
                                            * TabContentManager.PIXEL_TOLERANCE_PERCENT);
                }
            }
            assertTrue("should have at least one valid ViewHolder", hasAtLeastOneValidViewHolder);
        }
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    @DisabledTest(message = "crbug.com/1096997")
    public void testTabGroupManualSelection() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabListEditorTestingRobot robot = new TabListEditorTestingRobot();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(3));

        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Group tabs");

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabListEditorIsHidden();
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testTabListEditor_SystemBackDismiss() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabListEditorTestingRobot robot = new TabListEditorTestingRobot();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        onView(tabSwitcherViewMatcher()).check(TabCountAssertion.havingTabCount(2));
        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        // Pressing system back should dismiss the selection editor.
        Espresso.pressBack();
        robot.resultRobot.verifyTabListEditorIsHidden();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({ChromeFeatureList.ARCHIVE_TAB_SERVICE + "<Study"})
    @DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    @CommandLineFlags.Add({
        BASE_PARAMS
                + "/baseline_tab_suggestions/true"
                + "/baseline_archive_tab_service/true/min_time_between_prefetches/0"
    })
    @DisabledTest(message = "https://crbug.com/1449985")
    public void testTabGroupManualSelection_AfterReviewTabSuggestion() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabListEditorTestingRobot robot = new TabListEditorTestingRobot();
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
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        robot.resultRobot
                .verifyTabListEditorIsVisible()
                .verifyToolbarActionViewEnabled(R.id.tab_list_editor_close_menu_item);

        robot.actionRobot.clickToolbarActionView(R.id.tab_list_editor_close_menu_item);
        robot.resultRobot.verifyTabListEditorIsHidden();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getCurrentTabModel().getCount(),
                            Matchers.is(0));
                });

        // Show Manual Selection Mode.
        createTabs(cta, false, 3);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Group tabs");

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabListEditorIsHidden();
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @DisabledTest(message = "crbug.com/1187320 This doesn't work with FeedV2 and crbug.com/1096295")
    public void testActivityCanBeGarbageCollectedAfterFinished() {
        prepareTabs(1, 0, "about:blank");

        WeakReference<ChromeTabbedActivity> activityRef =
                new WeakReference<>(mActivityTestRule.getActivity());

        ChromeTabbedActivity activity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mTabSwitcherLayout = null;
        mTabListDelegate = null;
        mActivityTestRule.setActivity(activity);

        // A longer timeout is needed. Achieve that by using the CriteriaHelper.pollUiThread.
        CriteriaHelper.pollUiThread(
                () -> GarbageCollectionTestUtils.canBeGarbageCollected(activityRef));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    public void verifyTabGroupStateAfterReparenting() throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(
                cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter);
        mActivityTestRule.loadUrl(mUrl);
        Tab parentTab = cta.getTabModelSelector().getCurrentTab();

        // Create a tab group.
        TabCreator tabCreator =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> cta.getTabCreator(false));
        LoadUrlParams loadUrlParams = new LoadUrlParams(mUrl);
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabCreator.createNewTab(
                                loadUrlParams,
                                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                                parentTab));
        Tab childTab = cta.getTabModelSelector().getCurrentModel().getTabAt(1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        TestThreadUtils.runOnUiThreadBlocking(() -> filter.moveTabOutOfGroup(childTab.getId()));
        verifyTabSwitcherCardCount(cta, 2);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(true));
        final ChromeTabbedActivity ctaNightMode =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        assertTrue(ColorUtils.inNightMode(ctaNightMode));
        CriteriaHelper.pollUiThread(ctaNightMode.getTabModelSelector()::isTabStateInitialized);
        enterTabSwitcher(ctaNightMode);
        verifyTabSwitcherCardCount(ctaNightMode, 2);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION})
    public void testUndoClosure_AccessibilityMode() {
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
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 2);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
    }

    @Test
    @MediumTest
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
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupColorInTabSwitcher() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);

        // Expect that the the dialog is dismissed via another action.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationDialogResultAction", 2);

        verifyGroupCreationDialogOpenedAndDismiss(cta);
        // Verify the color icon exists.
        onView(allOf(withId(R.id.tab_favicon), withParent(withId(R.id.card_view))))
                .check(matches(isDisplayed()));
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupCreation_acceptInputValues() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        // Verify the creation dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title.
        editGroupCreationDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Expect a changed color and title selection to be recorded and an acceptance action.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.TabGroupParity.TabGroupCreationFinalSelections", 3)
                        .expectIntRecord(
                                "Android.TabGroupParity.TabGroupCreationDialogResultAction", 0)
                        .build();

        // Accept the change.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("Test");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.BLUE);
        histograms.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupCreation_acceptNullTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        // Verify the creation dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Close the soft keyboard that appears when the dialog is shown.
        closeSoftKeyboard();

        // Expect changed color and title selection to be recorded.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationFinalSelections", 0);

        // Accept without changing the title.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();

        // Check that the title change is reflected.
        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupCreation_dismissEmptyTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        // Verify the creation dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Close the soft keyboard that appears when the dialog is shown.
        closeSoftKeyboard();

        // Change the title.
        editGroupCreationDialogTitle(cta, "");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Enact a backpress to dismiss the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change has reverted to the default null state.
        verifyFirstCardTitle("2 tabs");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupCreation_rejectInvalidTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        // Verify the creation dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title and accept.
        editGroupCreationDialogTitle(cta, "");
        onView(withId(R.id.positive_button)).perform(click());

        // Verify that the change was rejected and the dialog is still showing.
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Exit the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testTabGroupCreation_dismissSavesState() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        // Verify the creation dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title.
        editGroupCreationDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Expect that the dismiss action is recorded.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationDialogResultAction", 1);

        // Enact a backpress to dismiss the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("Test");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.BLUE);
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testLongPressTab_entryInTabSwitcher_verifyNoSelectionOccurs() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();

        // Verify no selection action occurred to switch the selected tab in the tab model
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(1));
    }

    @Test
    @MediumTest
    public void testLongPressTabGroup_entryInTabSwitcher() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testLongPressTab_verifyPostLongPressClickNoSelectionEditor() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0);
        Espresso.pressBack();

        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        // Check the selected tab in the tab model switches from the second tab to the first to
        // verify clicking the tab worked.
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(0));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_TabToGroupAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> filter.setTabGroupTitle(normalTabModel.getTabAt(0).getRootId(), "Foo"));
        verifyTabSwitcherCardCount(cta, 2);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(0).getRootId()));

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert the default color is still the tab group color
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(0).getRootId()));

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals("3", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 2);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Foo",
                            TabGroupTitleUtils.getTabGroupTitle(
                                    normalTabModel.getTabAt(1).getRootId()));
                    assertNull(
                            TabGroupTitleUtils.getTabGroupTitle(
                                    normalTabModel.getTabAt(2).getRootId()));
                    assertEquals(
                            nextSuggestedColorId,
                            TabGroupColorUtils.getTabGroupColor(
                                    normalTabModel.getTabAt(1).getRootId()));
                    assertEquals(
                            INVALID_COLOR_ID,
                            TabGroupColorUtils.getTabGroupColor(
                                    normalTabModel.getTabAt(2).getRootId()));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_GroupToGroupNonAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId1 = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge last two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(3), normalTabModel.getTabAt(4)));
        createTabGroup(cta, false, tabGroup);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Assert default color 1 was set properly.
        assertEquals(
                nextSuggestedColorId1,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(4).getRootId()));

        // Get the next suggested color id.
        int nextSuggestedColorId2 =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        List<Tab> tabGroup2 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup2);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 3);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    filter.setTabGroupTitle(normalTabModel.getTabAt(3).getRootId(), "Foo");
                    filter.setTabGroupTitle(normalTabModel.getTabAt(1).getRootId(), "Bar");
                });

        // Assert default color 2 was set properly.
        assertEquals(
                nextSuggestedColorId2,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));

        // Merge the two tab groups into a group.
        List<Tab> tabGroup3 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(3)));
        createTabGroup(cta, false, tabGroup3);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert default color 2 was set as the overall merged group color.
        assertEquals(
                nextSuggestedColorId2,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(3).getRootId()));

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals("4", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Foo",
                            TabGroupTitleUtils.getTabGroupTitle(
                                    normalTabModel.getTabAt(4).getRootId()));
                    assertEquals(
                            "Bar",
                            TabGroupTitleUtils.getTabGroupTitle(
                                    normalTabModel.getTabAt(0).getRootId()));
                    assertEquals(
                            nextSuggestedColorId1,
                            TabGroupColorUtils.getTabGroupColor(
                                    normalTabModel.getTabAt(4).getRootId()));
                    assertEquals(
                            nextSuggestedColorId2,
                            TabGroupColorUtils.getTabGroupColor(
                                    normalTabModel.getTabAt(0).getRootId()));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_PostMergeGroupTitleCommit() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        int[] ungroupedRootId = new int[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    filter.setTabGroupTitle(normalTabModel.getTabAt(0).getRootId(), "Foo");
                    ungroupedRootId[0] = normalTabModel.getTabAt(2).getRootId();
                    filter.setTabGroupTitle(ungroupedRootId[0], "Bar");
                });
        verifyTabSwitcherCardCount(cta, 2);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert default color was set properly for the overall merged group.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(2).getRootId()));

        // Check that the old group title was handed over when the group merge is committed
        // and no longer exists.
        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(TabGroupTitleUtils.getTabGroupTitle(ungroupedRootId[0]));
                    assertEquals(
                            "Foo",
                            TabGroupTitleUtils.getTabGroupTitle(
                                    normalTabModel.getTabAt(0).getRootId()));
                });

        // Assert color still exists post snackbar dismissal.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoClosure_UndoGroupClosure() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());

        // Get the next suggested color id.
        int nextSuggestedColorId =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Temporarily save the tab to get the rootId later.
        Tab tab2 = normalTabModel.getTabAt(1);

        closeFirstTabInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);

        // Default color should still persist, though the root id might change.
        assertEquals(nextSuggestedColorId, TabGroupColorUtils.getTabGroupColor(tab2.getRootId()));

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color still persists.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoClosure_AcceptGroupClosure() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());

        // Get the next suggested color id.
        int nextSuggestedColorId =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyGroupCreationDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                TabGroupColorUtils.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Temporarily save the rootID to check during closure.
        Tab tab2 = normalTabModel.getTabAt(1);
        int groupRootId = tab2.getRootId();

        closeFirstTabInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);

        // Default color should still persist, though the root id might change.
        assertEquals(nextSuggestedColorId, TabGroupColorUtils.getTabGroupColor(tab2.getRootId()));

        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Assert default color is cleared.
        assertEquals(INVALID_COLOR_ID, TabGroupColorUtils.getTabGroupColor(groupRootId));
        assertEquals(INVALID_COLOR_ID, TabGroupColorUtils.getTabGroupColor(tab2.getRootId()));
    }

    // TODO(crbug/324919909): Delete this test once Hub is launched. It is migrated to
    // TabSwitcherPanePublicTransitTest as a combined testEmptyStateView case.
    @Test
    @MediumTest
    public void testEmptyStateView_DeleteLastNormalTab() throws Exception {
        prepareTabs(1, 0, NTP_URL);
        enterGTSWithThumbnailChecking();

        // Close the last tab.
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .closeTab(tab, false, false, true);
                });

        // Check empty view should show up.
        Context appContext = ApplicationProvider.getApplicationContext();
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        appContext)))))
                .check(matches(isDisplayed()));
    }

    // TODO(crbug/324919909): Delete this test once Hub is launched. It is migrated to
    // TabSwitcherPanePublicTransitTest as a combined testEmptyStateView case.
    @Test
    @MediumTest
    public void testEmptyStateView_ToggleIncognito() {
        mActivityTestRule.loadUrl(mUrl);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Prepare one incognito tab and one normal and enter tab switcher.
        createTabs(cta, true, 1);
        createTabs(cta, false, 1);
        enterTabSwitcher(cta);

        // Go into normal tab switcher.
        switchTabModel(cta, false);

        // Close the last normal tab.
        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .closeTab(tab, false, false, true);
                });

        // Go into incognito tab switcher.
        switchTabModel(cta, true);

        // Check empty view should never show up in incognito tab switcher.
        Context appContext = ApplicationProvider.getApplicationContext();
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        appContext)))))
                .check(matches(not(isDisplayed())));

        // Close the last incognito tab.
        Tab incognitoTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .closeTab(incognitoTab, false, false, true);
                });

        // Incognito tab switcher should exit to go to normal tab switcher and we should see empty
        // view.
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        appContext)))))
                .check(matches(isDisplayed()));
    }

    // TODO(crbug/324919909): Delete this test once Hub is launched. It is covered in
    // HubLayoutUnitTest.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testHideTabOnTabSwitcher() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrlInNewTab("about:blank");
        final Tab currentTab = cta.getActivityTab();
        final CallbackHelper shownHelper = new CallbackHelper();
        final CallbackHelper hiddenHelper = new CallbackHelper();
        TabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        assertEquals("Unexpected tab shown", tab, currentTab);
                        shownHelper.notifyCalled();
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        assertEquals("Unexpected tab hidden", tab, currentTab);
                        assertEquals(
                                "Unexpected hiding type", type, TabHidingType.TAB_SWITCHER_SHOWN);
                        hiddenHelper.notifyCalled();
                    }
                };

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    currentTab.addObserver(observer);
                });

        enterTabSwitcher(cta);
        hiddenHelper.waitForFirst();

        leaveTabSwitcher(cta);
        shownHelper.waitForFirst();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    currentTab.removeObserver(observer);
                });
    }

    private TabSwitcher.TabListDelegate getTabListDelegateFromUIThread() {
        AtomicReference<TabSwitcher.TabListDelegate> tabListDelegate = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabListDelegate.set(
                                mTabSwitcherLayout
                                        .getTabSwitcherForTesting()
                                        .getTabListDelegate()));
        return tabListDelegate.get();
    }

    private void enterTabListEditor(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_select_tabs);
    }

    /** TODO(wychen): move some of the callers to {@link TabUiTestHelper#enterTabSwitcher}. */
    private void enterGTSWithThumbnailChecking() throws InterruptedException {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mActivityTestRule
                                .getActivity()
                                .getTabContentManager()
                                .removeTabThumbnail(currentTab.getId());
                    });
        }

        waitForCaptureRateControl();
        // TODO(wychen): use TabUiTestHelper.enterTabSwitcher() instead.
        //  Might increase flakiness though. See crbug.com/1024742.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, true);

        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
    }

    /** Like {@link TabUiTestHelper#enterTabSwitcher}, but make sure all tabs have thumbnail. */
    private void enterGTSWithThumbnailRetry() {
        enterTabSwitcher(mActivityTestRule.getActivity());
        try {
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        } catch (AssertionError ae) {
            // If the last thumbnail is missing, try without animation.
            Espresso.pressBack();
            TestThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mActivityTestRule
                                    .getActivity()
                                    .getLayoutManager()
                                    .showLayout(LayoutType.TAB_SWITCHER, false));
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        }
    }

    /**
     * If thumbnail checking is not needed, use {@link TabUiTestHelper#leaveTabSwitcher} instead.
     */
    private void leaveGTSAndVerifyThumbnailsAreReleased() throws InterruptedException {
        assertTrue(
                mActivityTestRule
                        .getActivity()
                        .getLayoutManager()
                        .isLayoutVisible(LayoutType.TAB_SWITCHER));

        TabUiTestHelper.pressBackOnTabSwitcher(mTabSwitcherLayout);
        // TODO(wychen): using default timeout or even converting to
        //  OverviewModeBehaviorWatcher shouldn't increase flakiness.
        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        assertThumbnailsAreReleased();
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

    private void simulateJpegHasCachedWithAspectRatio(double aspectRatio) throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        int jpegWidth = 125;
        int jpegHeight = (int) (jpegWidth * 1.0 / aspectRatio);
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = Bitmap.createBitmap(jpegWidth, jpegHeight, Config.ARGB_8888);
            encodeJpeg(tab, bitmap);
        }
    }

    private void simulateJpegHasCachedWithDefaultAspectRatio() throws IOException {
        simulateJpegHasCachedWithAspectRatio(
                TabUtils.getTabThumbnailAspectRatio(
                        mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().getBrowserControlsManager()));
    }

    private void simulateAspectRatioChangedToPoint75() throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = TabContentManager.getJpegForTab(tab.getId(), null);
            bitmap =
                    Bitmap.createScaledBitmap(
                            bitmap,
                            bitmap.getWidth(),
                            (int) (bitmap.getWidth() * 1.0 / 0.75),
                            false);
            encodeJpeg(tab, bitmap);
        }
    }

    private void encodeJpeg(Tab tab, Bitmap bitmap) throws IOException {
        FileOutputStream outputStream =
                new FileOutputStream(TabContentManager.getTabThumbnailFileJpeg(tab.getId()));
        bitmap.compress(Bitmap.CompressFormat.JPEG, 50, outputStream);
        outputStream.close();
    }

    private void verifyOnlyOneTabSuggestionMessageCardIsShowing() throws InterruptedException {
        String suggestionMessageTemplate =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.tab_suggestion_close_stale_message);
        String suggestionMessage =
                String.format(Locale.getDefault(), suggestionMessageTemplate, "3");
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(getTabCountInCurrentTabModel(), Matchers.is(3)));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
        leaveGTSAndVerifyThumbnailsAreReleased();

        // With soft or hard clean up depends on the soft-cleanup-delay and cleanup-delay params.
        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        // This will fail with error "matched multiple views" when there is more than one suggestion
        // message card.
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
    }

    private Matcher<View> tabSwitcherViewMatcher() {
        return allOf(
                isDescendantOfA(
                        withId(
                                TabUiTestHelper.getTabSwitcherAncestorId(
                                        mActivityTestRule.getActivity()))),
                withId(R.id.tab_list_recycler_view));
    }

    private void editGroupCreationDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(withId(R.id.title_input_text))
                .perform(click())
                .perform(replaceText(title))
                .perform(pressImeActionButton());
        // Wait until the keyboard is hidden to make sure the edit has taken effect.
        KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        CriteriaHelper.pollUiThread(
                () -> !delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
    }

    private void verifyFirstCardTitle(String title) {
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        mActivityTestRule.getActivity()))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            RecyclerView recyclerView = (RecyclerView) v;
                            TextView firstCardTitleTextView =
                                    recyclerView
                                            .findViewHolderForAdapterPosition(0)
                                            .itemView
                                            .findViewById(R.id.tab_title);
                            assertEquals(title, firstCardTitleTextView.getText().toString());
                        });
    }

    private void verifyFirstCardColor(@TabGroupColorId int color) {
        onView(allOf(withId(R.id.tab_favicon), withParent(withId(R.id.card_view))))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            ImageView imageView = (ImageView) v;
                            LayerDrawable layerDrawable = (LayerDrawable) imageView.getDrawable();
                            GradientDrawable drawable =
                                    (GradientDrawable) layerDrawable.getDrawable(1);

                            assertEquals(
                                    ColorStateList.valueOf(
                                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                                    mActivityTestRule.getActivity(), color, false)),
                                    drawable.getColor());
                        });
    }

    private void verifyGroupCreationDialogOpenedAndDismiss(ChromeTabbedActivity cta) {
        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the creation dialog exists.
        onViewWaiting(withId(R.id.creation_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Wait until the keyboard is showing.
        KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        CriteriaHelper.pollUiThread(
                () -> delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
        // Dismiss the tab group creation dialog.
        dismissAllModalDialogs();
        // Verify that the modal dialog is now hidden.
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
    }

    private void verifyModalDialogShowingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                });
    }

    private void verifyModalDialogHidingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(false));
                });
    }

    private void dismissAllModalDialogs() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
    }
}
