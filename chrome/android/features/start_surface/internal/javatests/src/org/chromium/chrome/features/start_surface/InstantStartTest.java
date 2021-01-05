// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.core.AllOf;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.BuildConfig;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.StreamUtil;
import org.chromium.base.SysUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feed.FeedV1;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateFileManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelMetadata;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0";
    private static final int ARTICLE_SECTION_HEADER_POSITION = 0;
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public ErrorCollector collector = new ErrorCollector();

    /**
     * Parameter set controlling whether Feed v2 is enabled.
     */
    public static class FeedParams implements ParameterProvider {
        private static List<ParameterSet> sFeedParams =
                Arrays.asList(new ParameterSet().value(false).name("FeedV1"),
                        new ParameterSet().value(true).name("FeedV2"));

        @Override
        public List<ParameterSet> getParameters() {
            return sFeedParams;
        }
    }

    private void setFeedVersion(boolean isFeedV2) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INTEREST_FEED_V2, isFeedV2);
    }

    /**
     * Only launch Chrome without waiting for a current tab.
     * This test could not use {@link ChromeActivityTestRule#startMainActivityFromLauncher()}
     * because of its {@link org.chromium.chrome.browser.tab.Tab} dependency.
     */
    private void startMainActivityFromLauncher() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.launchActivity(intent);
    }

    public static Bitmap createThumbnailBitmapAndWriteToFile(int tabId) {
        final Bitmap thumbnailBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        try {
            File thumbnailFile = TabContentManager.getTabThumbnailFileJpeg(tabId);
            if (thumbnailFile.exists()) {
                thumbnailFile.delete();
            }
            Assert.assertFalse(thumbnailFile.exists());

            FileOutputStream thumbnailFileOutputStream = new FileOutputStream(thumbnailFile);
            thumbnailBitmap.compress(Bitmap.CompressFormat.JPEG, 100, thumbnailFileOutputStream);
            thumbnailFileOutputStream.flush();
            thumbnailFileOutputStream.close();

            Assert.assertTrue(thumbnailFile.exists());
        } catch (IOException e) {
            e.printStackTrace();
        }
        return thumbnailBitmap;
    }

    private void createCorruptedTabStateFile() throws IOException {
        File stateFile = new File(TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                TabbedModeTabPersistencePolicy.getStateFileName(0));
        FileOutputStream output = new FileOutputStream(stateFile);
        output.write(0);
        output.close();
    }

    /**
     * Create a file so that a TabState can be restored later.
     * @param tabId the Tab ID
     * @param encrypted for Incognito mode
     */
    private static void saveTabState(int tabId, boolean encrypted) {
        File file = TabStateFileManager.getTabStateFile(
                TabStateDirectory.getOrCreateTabbedModeStateDirectory(), tabId, encrypted);
        writeFile(file, M26_GOOGLE_COM.encodedTabState);

        TabState tabState = TabStateFileManager.restoreTabState(file, false);
        tabState.rootId = PseudoTab.fromTabId(tabId).getRootId();
        TabStateFileManager.saveState(file, tabState, encrypted);
    }

    private static void writeFile(File file, String data) {
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (Exception e) {
            assert false : "Failed to create " + file;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    /**
     * Create all the files so that tab models can be restored
     * @param tabIds all the Tab IDs in the normal tab model.
     */
    public static void createTabStateFile(int[] tabIds) throws IOException {
        createTabStateFile(tabIds, 0);
    }

    /**
     * Create all the files so that tab models can be restored
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param selectedIndex the selected index of normal tab model.
     */
    public static void createTabStateFile(int[] tabIds, int selectedIndex) throws IOException {
        TabModelMetadata normalInfo = new TabModelMetadata(selectedIndex);
        for (int tabId : tabIds) {
            normalInfo.ids.add(tabId);
            normalInfo.urls.add("");

            saveTabState(tabId, false);
        }
        TabModelMetadata incognitoInfo = new TabModelMetadata(0);

        byte[] listData = TabPersistentStore.serializeMetadata(normalInfo, incognitoInfo);

        File stateFile = new File(TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                TabbedModeTabPersistencePolicy.getStateFileName(0));
        FileOutputStream output = new FileOutputStream(stateFile);
        output.write(listData);
        output.close();
    }

    private void startAndWaitNativeInitialization() {
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().startDelayedNativeInitializationForTests());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized,
                ScalableTimeout.scaleTimeout(10000L), CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }

    private StartSurfaceCoordinator getStartSurfaceFromUIThread() {
        AtomicReference<StartSurface> startSurface = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurface.set(
                    ((ChromeTabbedActivity) mActivityTestRule.getActivity()).getStartSurface());
        });
        return (StartSurfaceCoordinator) startSurface.get();
    }

    /**
     * Test TabContentManager is able to fetch thumbnail jpeg files before native is initialized.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:allow_to_refetch/true/thumbnail_aspect_ratio/2.0"})
    public void fetchThumbnailsPreNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertTrue(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION.getValue());

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

        final Bitmap thumbnailBitmap = createThumbnailBitmapAndWriteToFile(tabId);
        tabContentManager.getTabThumbnailWithCallback(tabId, thumbnailFetchListener, false, false);
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS})
    public void layoutManagerChromePhonePreNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
            ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single/enable_launch_polish/true"})
    public void startSurfaceSinglePanePreNativeAndWithNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);

        StartSurfaceCoordinator startSurfaceCoordinator = getStartSurfaceFromUIThread();
        Assert.assertTrue(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isInitializedWithNativeForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurfaceCoordinator.getController().setOverviewState(
                    StartSurfaceState.SHOWN_TABSWITCHER);
        });
        CriteriaHelper.pollUiThread(startSurfaceCoordinator::isSecondaryTaskInitPendingForTesting);

        // Initializes native.
        startAndWaitNativeInitialization();
        Assert.assertFalse(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());
        Assert.assertTrue(startSurfaceCoordinator.isInitializedWithNativeForTesting());
    }

    /**
     * Tests that the IncognitoSwitchCoordinator isn't create in inflate() if the native library
     * isn't ready. It will be lazily created after native initialization.
     */
    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void startSurfaceToolbarInflatedPreAndWithNativeTest() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        TopToolbarCoordinator topToolbarCoordinator =
                (TopToolbarCoordinator) mActivityTestRule.getActivity()
                        .getToolbarManager()
                        .getToolbar();

        onViewWaiting(allOf(withId(R.id.tab_switcher_toolbar), isDisplayed()));

        StartSurfaceToolbarCoordinator startSurfaceToolbarCoordinator =
                topToolbarCoordinator.getStartSurfaceToolbarForTesting();
        // Verifies that the IncognitoSwitchCoordinator hasn't been created when the
        // {@link StartSurfaceToolbarCoordinator#inflate()} is called.
        Assert.assertNull(startSurfaceToolbarCoordinator.getIncognitoSwitchCoordinatorForTesting());

        // Initializes native.
        startAndWaitNativeInitialization();
        Assert.assertNotNull(
                startSurfaceToolbarCoordinator.getIncognitoSwitchCoordinatorForTesting());
    }

    /**
     * Tests that clicking the "more_tabs" button won't make Omnibox get focused when single tab is
     * shown on the StartSurface.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
            "/start_surface_variation/single/show_last_active_tab_only/true"})
    public void startSurfaceMoreTabsButtonTest() throws IOException {
        // clang-format on
        createTabStateFile(new int[] {0});
        createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());
        Assert.assertFalse(
                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue());

        mActivityTestRule.waitForActivityNativeInitializationComplete();

        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1025296): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(()
                                                          -> mActivityTestRule.getActivity()
                                                                     .findViewById(R.id.more_tabs)
                                                                     .performClick());
        } catch (ExecutionException e) {
            Assert.fail("Failed to tap 'more tabs' " + e.toString());
        }

        onViewWaiting(withId(R.id.tab_list_view));
        Assert.assertFalse(mActivityTestRule.getActivity().findViewById(R.id.url_bar).isFocused());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void willInitNativeOnTabletTest() {
        startMainActivityFromLauncher();
        Assert.assertTrue(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityTestRule.getActivity().getLayoutManager(), notNullValue());
        });

        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        assertThat(mActivityTestRule.getActivity().getLayoutManager())
                .isInstanceOf(LayoutManagerChromeTablet.class);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    public void testShouldShowStartSurfaceAsTheHomePagePreNative() {
        // clang-format on
        Assert.assertTrue(StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled());
        Assert.assertFalse(TextUtils.isEmpty(HomepageManager.getHomepageUri()));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) ReturnToChromeExperimentsUtil::shouldShowStartSurfaceAsTheHomePage);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/omniboxonly"})
    public void renderTabSwitcher_NoStateFile() throws IOException {
        // clang-format on
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_empty");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/omniboxonly"})
    public void renderTabSwitcher_CorruptedStateFile() throws IOException {
        // clang-format on
        createCorruptedTabStateFile();
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_empty");
    }

    private boolean allCardsHaveThumbnail(RecyclerView recyclerView) {
        RecyclerView.Adapter adapter = recyclerView.getAdapter();
        assert adapter != null;
        for (int i = 0; i < adapter.getItemCount(); i++) {
            RecyclerView.ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder != null) {
                ImageView thumbnail = viewHolder.itemView.findViewById(R.id.tab_thumbnail);
                if (!(thumbnail.getDrawable() instanceof BitmapDrawable)) return false;
                BitmapDrawable drawable = (BitmapDrawable) thumbnail.getDrawable();
                Bitmap bitmap = drawable.getBitmap();
                if (bitmap == null) return false;
            }
        }
        return true;
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/omniboxonly"})
    public void renderTabSwitcher() throws IOException, InterruptedException {
        // clang-format on
        if (!FeedV1.IS_AVAILABLE) return; // Test not yet working for FeedV2.

        createTabStateFile(new int[] {0, 1, 2});
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        createThumbnailBitmapAndWriteToFile(2);
        TabAttributeCache.setTitleForTesting(0, "title");
        TabAttributeCache.setTitleForTesting(1, "漢字");
        TabAttributeCache.setTitleForTesting(2, "اَلْعَرَبِيَّةُ");

        // Must be after createTabStateFile() to read these files.
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);
        RecyclerView recyclerView =
                mActivityTestRule.getActivity().findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(3,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        // TODO(crbug.com/1065314): find a better way to wait for a stable rendering.
        Thread.sleep(2000);
        // The titles on the tab cards changes to "Google" because we use M26_GOOGLE_COM.
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs_postNative");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/omniboxonly"})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
            sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups() throws IOException, InterruptedException {
        // clang-format on
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        createThumbnailBitmapAndWriteToFile(2);
        createThumbnailBitmapAndWriteToFile(3);
        createThumbnailBitmapAndWriteToFile(4);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        TabAttributeCache.setRootIdForTesting(2, 0);
        TabAttributeCache.setRootIdForTesting(3, 3);
        TabAttributeCache.setRootIdForTesting(4, 3);
        // createTabStateFile() has to be after setRootIdForTesting() to get root IDs.
        createTabStateFile(new int[] {0, 1, 2, 3, 4});

        // Must be after createTabStateFile() to read these files.
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getLayoutManager()::overviewVisible);
        RecyclerView recyclerView =
                mActivityTestRule.getActivity().findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        // TODO(crbug.com/1065314): Tab group cards should not have favicons.
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(R.id.tab_list_view),
                "tabSwitcher_tabGroups");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(5,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        Assert.assertEquals(2,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getCount());
        Assert.assertEquals(3,
                getRelatedTabListSizeOnUiThread(mActivityTestRule.getActivity()
                                                        .getTabModelSelector()
                                                        .getTabModelFilterProvider()
                                                        .getCurrentTabModelFilter()));
        // TODO(crbug.com/1065314): fix thumbnail changing in post-native rendering and make sure
        //  post-native GTS looks the same.
    }

    private int getRelatedTabListSizeOnUiThread(TabModelFilter tabModelFilter)
            throws InterruptedException {
        AtomicInteger res = new AtomicInteger();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(tabModelFilter.getRelatedTabList(2).size()); });
        return res.get();
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        "force-fieldtrials=Study/Group",
        IMMEDIATE_RETURN_PARAMS +
            "/start_surface_variation/single" +
            "/exclude_mv_tiles/true" +
            "/hide_switch_when_no_incognito_tabs/true" +
            "/show_last_active_tab_only/true"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void renderSingleAsHomepage_SingleTabNoMVTiles(boolean isFeedV2)
        throws IOException, InterruptedException {
        // clang-format on
        setFeedVersion(isFeedV2);

        createTabStateFile(new int[] {0});
        createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        View surface =
                mActivityTestRule.getActivity().findViewById(R.id.primary_tasks_surface_view);

        ViewUtils.onViewWaiting(AllOf.allOf(withId(R.id.single_tab_view), isDisplayed()));
        ChromeRenderTestRule.sanitize(surface);
        // TODO(crbug.com/1065314): fix favicon.
        mRenderTestRule.render(
                surface, "singlePane_singleTab_noMV4" + (isFeedV2 ? "_FeedV2" : "_FeedV1"));

        // Initializes native.
        startAndWaitNativeInitialization();

        // TODO(crbug.com/1065314): fix login button animation in post-native rendering and
        //  make sure post-native single-tab card looks the same.
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO,
        ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        "force-fieldtrials=Study/Group",
        IMMEDIATE_RETURN_PARAMS +
            "/start_surface_variation/single" +
            "/exclude_mv_tiles/true" +
            "/show_last_active_tab_only/true" +
            "/show_stack_tab_switcher/true"})
    public void renderSingleAsHomepageV2_PageInfoIconShown()
        throws IOException {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        startAndWaitNativeInitialization();
        waitForTabModel();

        // Click on the search box. Omnibox should show up.
        onView(withId(R.id.search_box_text)).perform(click());
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);

        View surface = mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        ChromeRenderTestRule.sanitize(surface);
        mRenderTestRule.render(surface, "singleV2_omniboxClickedShowLogo");
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INTEREST_FEED_V2})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void testFeedPlaceholderFromColdStart(boolean isFeedV2) {
        // clang-format on
        setFeedVersion(isFeedV2);
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        // Feed placeholder should be shown from cold start with Instant Start on.
        StartSurfaceCoordinator startSurfaceCoordinator = getStartSurfaceFromUIThread();
        onView(withId(R.id.placeholders_layout)).check(matches(isDisplayed()));
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        startAndWaitNativeInitialization();
        // Feed background should be non-transparent finally.
        ViewUtils.onViewWaiting(
                AllOf.allOf(withId(R.id.feed_stream_recycler_view), matchesBackgroundAlpha(255)));

        // TODO(spdonghao): Add a test for Feed placeholder from warm start. It's tested in
        // StartSurfaceMediatorUnitTest#feedPlaceholderFromWarmStart currently because warm start is
        // hard to simulate here.
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INTEREST_FEED_V2})
    // clang-format off
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void testCachedFeedVisibility(boolean isFeedV2) {
        // clang-format on
        setFeedVersion(isFeedV2);
        startMainActivityFromLauncher();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        // FEED_ARTICLES_LIST_VISIBLE should equal to ARTICLES_LIST_VISIBLE.
        CriteriaHelper.pollUiThread(()
                                            -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                        == StartSurfaceConfiguration.getFeedArticlesVisibility());

        // Hide articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both false.
        toggleHeader(false);
        CriteriaHelper.pollUiThread(() -> !StartSurfaceConfiguration.getFeedArticlesVisibility());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                StartSurfaceConfiguration.getFeedArticlesVisibility()));

        // Show articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both true.
        toggleHeader(true);
        CriteriaHelper.pollUiThread(StartSurfaceConfiguration::getFeedArticlesVisibility);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                StartSurfaceConfiguration.getFeedArticlesVisibility()));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INTEREST_FEED_V2})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void testHidePlaceholder(boolean isFeedV2) {
        // clang-format on
        setFeedVersion(isFeedV2);
        StartSurfaceConfiguration.setFeedVisibilityForTesting(false);
        startMainActivityFromLauncher();

        // When cached Feed articles' visibility is invisible, placeholder should be invisible too.
        onView(withId(R.id.placeholders_layout)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INTEREST_FEED_V2})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void testShowPlaceholder(boolean isFeedV2) {
        // clang-format on
        setFeedVersion(isFeedV2);
        StartSurfaceConfiguration.setFeedVisibilityForTesting(true);
        startMainActivityFromLauncher();

        // When cached Feed articles' visibility is visible, placeholder should be visible too.
        onView(withId(R.id.placeholders_layout)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.DisableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
    public void testInstantStartWithoutStartSurface() throws IOException {
        createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(TabUiFeatureUtilities.supportInstantStart(false));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertFalse(StartSurfaceConfiguration.isStartSurfaceEnabled());

        Assert.assertEquals(1,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = mActivityTestRule.getActivity().getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.
    Add({ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER, "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void
    testInstantStartNotCrashWhenAccessibilityLayoutEnabled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        Assert.assertTrue(DeviceClassManager.enableAccessibilityLayout());

        createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertFalse(TabUiFeatureUtilities.supportInstantStart(false));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(StartSurfaceConfiguration.isStartSurfaceEnabled());

        Assert.assertEquals(1,
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = mActivityTestRule.getActivity().getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    // clang-format off
    public void  testInstantStartDisabledOnLowEndDevice() throws IOException {
        // clang-format on
        createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        // SysUtils.resetForTesting is required here due to the test restriction setup. With the
        // RESTRICTION_TYPE_NON_LOW_END_DEVICE restriction on the class, SysUtils#detectLowEndDevice
        // is called before the BaseSwitches.ENABLE_LOW_END_DEVICE_MODE is applied. Reset here to
        // make sure BaseSwitches.ENABLE_LOW_END_DEVICE_MODE can be applied.
        SysUtils.resetForTesting();

        Assert.assertFalse(TabUiFeatureUtilities.supportInstantStart(false));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
    public void testNoGURLPreNative() {
        // clang-format on
        if (!BuildConfig.DCHECK_IS_ON) return;

        collector.checkThat(StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled(), is(true));
        collector.checkThat(TextUtils.isEmpty(HomepageManager.getHomepageUri()), is(false));
        Assert.assertFalse(
                NativeLibraryLoadedStatus.getProviderForTesting().areMainDexNativeMethodsReady());
        ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage();
        ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePageNoTabs();
        PseudoTab.getAllPseudoTabsFromStateFile();

        Assert.assertFalse("There should be no GURL usages triggering native library loading",
                NativeLibraryLoadedStatus.getProviderForTesting().areMainDexNativeMethodsReady());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void renderSingleAsHomepage_NoTab_ScrollToolbarToTop() throws IOException {
        // clang-format on
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        // Initializes native.
        startAndWaitNativeInitialization();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 0, 0);

        /** Drag the {@link R.id.placeholders_layout} to scroll the toolbar to the top. */
        int toY = -mActivityTestRule.getActivity().getResources().getDimensionPixelOffset(
                org.chromium.chrome.start_surface.R.dimen.toolbar_height_no_shadow);
        TestTouchUtils.dragCompleteView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().findViewById(R.id.placeholders_layout), 0, 0, 0,
                toY, 1);

        // The start surface toolbar should be scrolled up and not be displayed.
        onView(withId(org.chromium.chrome.start_surface.R.id.tab_switcher_toolbar))
                .check(matches(not(isDisplayed())));

        // Toolbar container view should show.
        onView(withId(org.chromium.chrome.start_surface.R.id.toolbar_container))
                .check(matches(isDisplayed()));

        View surface = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ChromeRenderTestRule.sanitize(surface);
        // TODO(crbug.com/1065314): fix favicon.
        mRenderTestRule.render(surface, "singlePane_floatingTopToolbar");
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShadowVisibility() throws IOException {
        // clang-format on
        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        int shadowVisibility =
                mActivityTestRule.getActivity().findViewById(R.id.toolbar_shadow).getVisibility();

        Assert.assertEquals(View.INVISIBLE, shadowVisibility);
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShadowVisibilityWithoutInstantStart() throws IOException {
        // clang-format on
        startMainActivityFromLauncher();
        onViewWaiting(withId(R.id.toolbar_shadow)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testSingleAsHomepage_CloseTabInCarouselTabSwitcher()
            throws IOException, ExecutionException {
        // clang-format on
        createTabStateFile(new int[] {0});
        createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        // Initializes native.
        startAndWaitNativeInitialization();
        waitForTabModel();
        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 1, 0);

        onView(withId(R.id.tab_list_view)).check(matches(isDisplayed()));
        RecyclerView tabListView = mActivityTestRule.getActivity().findViewById(R.id.tab_list_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabListView.getChildAt(0).findViewById(R.id.action_button).performClick());

        TabUiTestHelper.verifyTabModelTabCount(mActivityTestRule.getActivity(), 0, 0);
        assertEquals(mActivityTestRule.getActivity()
                             .findViewById(R.id.tab_switcher_title)
                             .getVisibility(),
                View.GONE);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @ParameterAnnotations.UseMethodParameter(FeedParams.class)
    public void renderSingleAsHomepage_Landscape(boolean isFeedV2) throws IOException {
        // clang-format on
        setFeedVersion(isFeedV2);

        createTabStateFile(new int[] {0, 1, 2});

        startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        // Initializes native.
        startAndWaitNativeInitialization();
        onViewWaiting(
                allOf(withId(org.chromium.chrome.start_surface.R.id.feed_stream_recycler_view),
                        isDisplayed()));

        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().getResources().getConfiguration().orientation,
                    is(ORIENTATION_LANDSCAPE));
        });
        View surface =
                mActivityTestRule.getActivity().findViewById(R.id.primary_tasks_surface_view);
        mRenderTestRule.render(
                surface, "singlePane_landscape" + (isFeedV2 ? "_FeedV2" : "_FeedV1"));
    }

    /**
     * Toggles the header and checks whether the header has the right status.
     *
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(boolean expanded) {
        if (FeedFeatures.isV2Enabled()) {
            onView(allOf(instanceOf(RecyclerView.class), withId(R.id.feed_stream_recycler_view)))
                    .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
            onView(withId(R.id.header_menu)).perform(click());

            onView(withText(expanded ? R.string.ntp_turn_on_feed : R.string.ntp_turn_off_feed))
                    .perform(click());

            onView(withText(expanded ? R.string.ntp_discover_on : R.string.ntp_discover_off))
                    .check(matches(isDisplayed()));
        } else {
            onView(allOf(instanceOf(RecyclerView.class), withId(R.id.feed_stream_recycler_view)))
                    .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION),
                            RecyclerViewActions.actionOnItemAtPosition(
                                    ARTICLE_SECTION_HEADER_POSITION, click()));

            waitForView((ViewGroup) mActivityTestRule.getActivity().findViewById(
                                R.id.feed_stream_recycler_view),
                    allOf(withId(R.id.header_status),
                            withText(expanded ? R.string.hide_content : R.string.show_content)));
        }
    }

    public static Matcher<View> matchesBackgroundAlpha(final int expectedAlpha) {
        return new BoundedMatcher<View, View>(View.class) {
            String mMessage;
            int mActualAlpha;

            @Override
            protected boolean matchesSafely(View item) {
                if (item.getBackground() == null) {
                    mMessage = item.getId() + " does not have a background";
                    return false;
                }
                mActualAlpha = item.getBackground().getAlpha();
                return mActualAlpha == expectedAlpha;
            }
            @Override
            public void describeTo(final Description description) {
                if (expectedAlpha != mActualAlpha) {
                    mMessage = "Background alpha did not match: Expected " + expectedAlpha + " was "
                            + mActualAlpha;
                }
                description.appendText(mMessage);
            }
        };
    }

    private void waitForTabModel() {
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }
}
