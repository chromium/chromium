// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.Size;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.SysUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup. See
 * {@link InstantStartToolbarTest}, {@link InstantStartFeedTest}, {@link
 * InstantStartTabSwitcherTest}, {@link InstantStartNewTabFromLauncherTest} for more tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
@DoNotBatch(reason = "InstantStartTest tests startup behaviours and thus can't be batched.")
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS_PARAM + "/0";
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    @Rule
    public ErrorCollector collector = new ErrorCollector();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Before
    public void setUp() {
        ReturnToChromeUtil.setSkipInitializationCheckForTesting(true);
    }

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    /**
     * Test TabContentManager is able to fetch thumbnail jpeg files before native is initialized.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    // clang-format off
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrial-params=Study.Group:thumbnail_aspect_ratio/2.0"})
    public void fetchThumbnailsPreNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);

        int tabId = 0;
        mThumbnailFetchCount = 0;
        Callback<Bitmap> thumbnailFetchListener = (bitmap) -> {
            mThumbnailFetchCount++;
            mBitmap = bitmap;
        };

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().getTabContentManager(), notNullValue());
        });

        TabContentManager tabContentManager =
                mActivityTestRule.getActivity().getTabContentManager();

        final Bitmap thumbnailBitmap =
                StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(tabId);
        tabContentManager.getTabThumbnailWithCallback(
                tabId, new Size(0, 0), thumbnailFetchListener, false, false);
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(mThumbnailFetchCount, greaterThan(0)));

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        Assert.assertEquals(1, mThumbnailFetchCount);
        Bitmap fetchedThumbnail = mBitmap;
        Assert.assertEquals(thumbnailBitmap.getByteCount(), fetchedThumbnail.getByteCount());
        Assert.assertEquals(thumbnailBitmap.getWidth(), fetchedThumbnail.getWidth());
        Assert.assertEquals(thumbnailBitmap.getHeight(), fetchedThumbnail.getHeight());
    }

    @Test
    @SmallTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION, IMMEDIATE_RETURN_PARAMS})
    public void layoutManagerChromePhonePreNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromePhone.class);
        TabUiTestHelper.verifyTabSwitcherLayoutType(cta);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.IsHomepagePolicyManagerInitialized"));
    }

    @Test
    @SmallTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
            ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    // TODO(https://crbug.com/1315676): Removes this test once the start surface refactoring is
    // done, since the secondary tasks surface will go away.
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/enable_launch_polish/true"})
    public void startSurfaceSinglePanePreNativeAndWithNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(cta.getLayoutManager().getOverviewLayout())
                .isInstanceOf(TabSwitcherAndStartSurfaceLayout.class);

        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        Assert.assertTrue(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isInitializedWithNativeForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurfaceCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        });
        CriteriaHelper.pollUiThread(startSurfaceCoordinator::isSecondaryTaskInitPendingForTesting);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        Assert.assertFalse(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());
        Assert.assertTrue(startSurfaceCoordinator.isInitializedWithNativeForTesting());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void willInitNativeOnTabletTest() {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertTrue(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());

        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat(cta.getLayoutManager(), notNullValue()); });

        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromeTablet.class);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    public void testShouldShowStartSurfaceAsTheHomePagePreNative() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        Assert.assertTrue(StartSurfaceConfiguration.isStartSurfaceFlagEnabled());
        Assert.assertFalse(TextUtils.isEmpty(HomepageManager.getHomepageUri()));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> ReturnToChromeUtil.shouldShowStartSurfaceAsTheHomePage(
                                mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
    public void testInstantStartWithoutStartSurface() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(cta));

        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testInstantStartNotCrashWhenAccessibilityLayoutEnabled() throws IOException {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));

        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        Assert.assertTrue(
                DeviceClassManager.enableAccessibilityLayout(mActivityTestRule.getActivity()));

        Assert.assertFalse(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(cta));
        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testInstantStartDisabledOnLowEndDevice() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        // SysUtils.resetForTesting is required here due to the test restriction setup. With the
        // RESTRICTION_TYPE_NON_LOW_END_DEVICE restriction on the class, SysUtils#detectLowEndDevice
        // is called before the BaseSwitches.ENABLE_LOW_END_DEVICE_MODE is applied. Reset here to
        // make sure BaseSwitches.ENABLE_LOW_END_DEVICE_MODE can be applied.
        SysUtils.resetForTesting();

        Assert.assertFalse(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
    public void testNoGURLPreNative() {
        // clang-format on
        if (!BuildConfig.ENABLE_ASSERTS) return;
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        collector.checkThat(StartSurfaceConfiguration.isStartSurfaceFlagEnabled(), is(true));
        collector.checkThat(TextUtils.isEmpty(HomepageManager.getHomepageUri()), is(false));
        Assert.assertFalse(
                NativeLibraryLoadedStatus.getProviderForTesting().areNativeMethodsReady());
        ReturnToChromeUtil.shouldShowStartSurfaceAsTheHomePage(mActivityTestRule.getActivity());
        ReturnToChromeUtil.isStartSurfaceEnabled(mActivityTestRule.getActivity());
        PseudoTab.getAllPseudoTabsFromStateFile(mActivityTestRule.getActivity());

        Assert.assertFalse("There should be no GURL usages triggering native library loading",
                NativeLibraryLoadedStatus.getProviderForTesting().areNativeMethodsReady());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void renderSingleAsHomepage_Landscape() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        onViewWaiting(allOf(withId(R.id.feed_stream_recycler_view), isDisplayed()));

        cta.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    cta.getResources().getConfiguration().orientation, is(ORIENTATION_LANDSCAPE));
        });
        View surface = cta.findViewById(R.id.primary_tasks_surface_view);
        mRenderTestRule.render(surface, "singlePane_landscapeV2");
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn() throws IOException {
        // clang-format on
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertEquals(
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getValue());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        testShowLastTabAtStartUp();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn_NoInstant()
          throws IOException {
        // clang-format on
        Assert.assertFalse(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertEquals(
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getValue());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        testShowLastTabAtStartUp();
    }

    private void testShowLastTabAtStartUp() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        // Launches Chrome and verifies that the last visited Tab is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        Assert.assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    startSurfaceCoordinator.getStartSurfaceState(), StartSurfaceState.NOT_SHOWN);
        });
    }

    private static List<Tile> createFakeSiteSuggestionTiles(List<SiteSuggestion> siteSuggestions) {
        List<Tile> suggestionTiles = new ArrayList<>();
        for (int i = 0; i < siteSuggestions.size(); i++) {
            suggestionTiles.add(new Tile(siteSuggestions.get(i), i));
        }
        return suggestionTiles;
    }

    private static void saveSiteSuggestionTilesToFile(List<SiteSuggestion> siteSuggestions)
            throws InterruptedException {
        // Get old file and ensure to delete it.
        File oldFile = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        Assert.assertTrue(oldFile.delete() && !oldFile.exists());

        // Save suggestion lists to file.
        final CountDownLatch latch = new CountDownLatch(1);
        MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                createFakeSiteSuggestionTiles(siteSuggestions), latch::countDown);

        // Wait util the file has been saved.
        latch.await();
    }
}
