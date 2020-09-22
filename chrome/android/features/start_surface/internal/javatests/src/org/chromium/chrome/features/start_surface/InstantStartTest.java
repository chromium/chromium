// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.SmallTest;

import org.hamcrest.core.AllOf;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.StreamUtil;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
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
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
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

    /**
     * Only launch Chrome without waiting for a current tab.
     * This test could not use {@link ChromeActivityTestRule#startMainActivityFromLauncher()}
     * because of its {@link org.chromium.chrome.browser.tab.Tab} dependency.
     */
    private void startMainActivityFromLauncher() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.startActivityCompletely(intent);
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
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
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
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
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

        StartSurfaceCoordinator startSurfaceCoordinator =
                (StartSurfaceCoordinator) ((ChromeTabbedActivity) mActivityTestRule.getActivity())
                        .getStartSurface();
        Assert.assertTrue(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isInitializedWithNativeForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurfaceCoordinator.getController().setOverviewState(
                    OverviewModeState.SHOWN_TABSWITCHER);
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
    public void renderSingleAsHomepage_SingleTabNoMVTiles()
        throws IOException, InterruptedException {
        // clang-format on
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
        mRenderTestRule.render(surface, "singlePane_singleTab_noMV3");

        // Initializes native.
        startAndWaitNativeInitialization();

        // TODO(crbug.com/1065314): fix login button animation in post-native rendering and
        //  make sure post-native single-tab card looks the same.
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testFeedLoading() {
        // clang-format on
        startMainActivityFromLauncher();
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        onView(withId(R.id.placeholders_layout)).check(matches(isDisplayed()));
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testFeedPlaceholderVisibility() {
        // clang-format on
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
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testHidePlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(false);
        startMainActivityFromLauncher();

        onView(withId(R.id.placeholders_layout)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShowPlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(true);
        startMainActivityFromLauncher();

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

    /**
     * Toggles the header and checks whether the header has the right status.
     *
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(boolean expanded) {
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
