// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.contextmenu.RevampedContextMenuUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.Clipboard;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for the Revamped Context Menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.GOOGLE_BASE_URL + "=http://example.com/"})
public class RevampedContextMenuTest implements DownloadTestRule.CustomMainActivityStart {
    // clang-format on
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String TEST_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";

    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    private static final String FILENAME_GIF = "download.gif";
    private static final String FILENAME_PNG = "test_image.png";
    private static final String FILENAME_WEBM = "test.webm";
    private static final String TEST_GIF_IMAGE_FILE_EXTENSION = ".gif";
    private static final String TEST_JPG_IMAGE_FILE_EXTENSION = ".jpg";

    // Test chip delegate that always returns valid chip render params.
    private static final ChipDelegate FAKE_CHIP_DELEGATE = new ChipDelegate() {
        @Override
        public boolean isChipSupported() {
            return true;
        }

        @Override
        public void getChipRenderParams(Callback<ChipRenderParams> callback) {
            // Do nothing.
        }

        @Override
        public void onMenuClosed() {
            // Do nothing.
        }

        @Override
        public boolean isValidChipRenderParams(ChipRenderParams chipRenderParams) {
            return true;
        }
    };

    private static final String[] TEST_FILES =
            new String[] {FILENAME_GIF, FILENAME_PNG, FILENAME_WEBM};

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }

    @Override
    public void customMainActivityStart() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);
        deleteTestFiles();
        mDownloadTestRule.startMainActivityWithURL(mTestUrl);
        mDownloadTestRule.assertWaitForPageScaleFactorMatch(0.5f);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        deleteTestFiles();
    }

    @Test
    @MediumTest
    public void testCopyLinkURL() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Allow DiskWrites temporarily in main thread to avoid
        // violation during copying under emulator environment.
        try (CloseableOnMainThread ignored = CloseableOnMainThread.StrictMode.allowDiskWrites()) {
            RevampedContextMenuUtils.selectContextMenuItem(
                    InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(),
                    tab, "testLink", R.id.contextmenu_copy_link_address);
        }

        assertStringContains("test_link.html", getClipboardText());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testCopyImageLinkCopiesLinkURL() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Allow DiskWrites temporarily in main thread to avoid
        // violation during copying under emulator environment.
        try (CloseableOnMainThread ignored = CloseableOnMainThread.StrictMode.allowDiskWrites()) {
            RevampedContextMenuUtils.selectContextMenuItem(
                    InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(),
                    tab, "testImageLink", R.id.contextmenu_copy_link_address);
        }

        assertStringContains("test_link.html", getClipboardText());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testSearchWithGoogleLensFiresIntent() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuUtils.selectContextMenuItemWithExpectedIntent(
                InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(), tab,
                "testImage", R.id.contextmenu_search_with_google_lens,
                "com.google.android.googlequicksearchbox");
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:lensShopVariation/ShopSimilarProducts"})
    public void
    testShopSimilarProductsFiresIntent() throws Throwable {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuUtils.selectContextMenuItemWithExpectedIntent(
                InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(), tab,
                "testImage", R.id.contextmenu_shop_similar_products,
                "com.google.android.googlequicksearchbox");
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
            + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/ShopImageWithGoogleLens"})
    public void
    testShopImageWithGoogleLensFiresIntent() throws Throwable {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuUtils.selectContextMenuItemWithExpectedIntent(
                InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(), tab,
                "testImage", R.id.contextmenu_shop_image_with_google_lens,
                "com.google.android.googlequicksearchbox");
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
            + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/SearchSimilarProducts"})
    public void
    testSearchSimilarProductsFiresIntent() throws Throwable {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuUtils.selectContextMenuItemWithExpectedIntent(
                InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(), tab,
                "testImage", R.id.contextmenu_search_similar_products,
                "com.google.android.googlequicksearchbox");
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/SearchSimilarProducts/minAgsaVersionNameForShopping/11.21"})
    public void
    testSearchSimilarProductsBelowShoppingMinimumAgsaVersion() throws Throwable {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        // Fallback to search with google lens when Agsa below minimum shopping supported version.
        RevampedContextMenuUtils.selectContextMenuItemWithExpectedIntent(
                InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(), tab,
                "testImage", R.id.contextmenu_search_with_google_lens,
                "com.google.android.googlequicksearchbox");
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    public void testLensShoppingAllowlistWithLensFeaturesDisabled() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testImage",
                R.id.contextmenu_search_by_image);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContextMenu.SelectedOptionAndroid.Image.ShoppingDomain"));
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testLongPressOnImage() throws TimeoutException {
        checkOpenImageInNewTab("testImage", "/chrome/test/data/android/contextmenu/test_image.png");
    }

    /**
     * @MediumTest
     * @Feature({"Browser"})
     * @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
     */
    @Test
    @FlakyTest(message = "http://crbug.com/606939")
    public void testLongPressOnImageLink() throws TimeoutException {
        checkOpenImageInNewTab(
                "testImageLink", "/chrome/test/data/android/contextmenu/test_image.png");
    }

    private void checkOpenImageInNewTab(String domId, final String expectedPath)
            throws TimeoutException {
        final Tab activityTab = mDownloadTestRule.getActivity().getActivityTab();

        final CallbackHelper newTabCallback = new CallbackHelper();
        final AtomicReference<Tab> newTab = new AtomicReference<>();
        mDownloadTestRule.getActivity().getTabModelSelector().addObserver(
                new TabModelSelectorObserver() {
                    @Override
                    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                        if (CriticalPersistedTabData.from(tab).getParentId()
                                != activityTab.getId()) {
                            return;
                        }
                        newTab.set(tab);
                        newTabCallback.notifyCalled();

                        mDownloadTestRule.getActivity().getTabModelSelector().removeObserver(this);
                    }
                });

        int callbackCount = newTabCallback.getCallCount();

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), activityTab, domId,
                R.id.contextmenu_open_image_in_new_tab);

        try {
            newTabCallback.waitForCallback(callbackCount);
        } catch (TimeoutException ex) {
            Assert.fail("New tab never created from context menu press");
        }

        // Only check for the URL matching as the tab will not be fully created in svelte mode.
        final String expectedUrl = mTestServer.getURL(expectedPath);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(newTab.get()),
                                Matchers.is(expectedUrl)));
    }

    @Test
    @MediumTest
    public void testDismissContextMenuOnBack() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(() -> {
            return !mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Context menu did not have window focus");

        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        CriteriaHelper.pollUiThread(() -> {
            return mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Activity did not regain focus.");
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    public void testLensTranslateChipNotShowingIfNotEnabled() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Chip popoup was initialized.",
                    menuCoordinator.getCurrentPopupWindowForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    public void testSelectLensTranslateChip() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            menuCoordinator.simulateTranslateImageClassificationForTesting();
            Assert.assertTrue("Chip popoup not showing.",
                    menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
            menuCoordinator.clickChipForTesting();
        });

        Assert.assertEquals("Selection histogram pings not equal to one", 1,
                RecordHistogram.getHistogramValueCountForTesting("ContextMenu.LensChip.Event",
                        RevampedContextMenuChipController.ChipEvent.CLICKED));
        Assert.assertFalse("Chip popoup still showing.",
                menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    public void testLensChipNotShowingAfterMenuDismissed() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Dismiss context menu.
        TestTouchUtils.singleClickView(InstrumentationRegistry.getInstrumentation(), tab.getView(),
                tab.getView().getWidth() - 5, tab.getView().getHeight() - 5);
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChipRenderParams chipRenderParams =
                    menuCoordinator.simulateImageClassificationForTesting();
            menuCoordinator.getChipRenderParamsCallbackForTesting(FAKE_CHIP_DELEGATE)
                    .bind(chipRenderParams)
                    .run();
            Assert.assertNull("Chip popoup was initialized.",
                    menuCoordinator.getCurrentPopupWindowForTesting());
        });
    }

    // Assert that focus is unchanged and that the chip popup does not block the dismissal of the
    // context menu.
    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    public void testDismissContextMenuOnClickLensTranslateChipEnabled() throws TimeoutException {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> menuCoordinator.simulateTranslateImageClassificationForTesting());
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(() -> {
            return !mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Context menu did not have window focus");

        TestTouchUtils.singleClickView(InstrumentationRegistry.getInstrumentation(), tab.getView(),
                tab.getView().getWidth() - 5, tab.getView().getHeight() - 5);

        CriteriaHelper.pollUiThread(() -> {
            return mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Activity did not regain focus.");
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP})
    public void testLensShoppingChipNotShowingIfNotEnabled() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Chip popoup was initialized.",
                    menuCoordinator.getCurrentPopupWindowForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP})
    public void testSelectLensShoppingChip() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            menuCoordinator.simulateShoppyImageClassificationForTesting();
            Assert.assertTrue("Chip popoup not showing.",
                    menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
            menuCoordinator.clickChipForTesting();
        });

        Assert.assertEquals("Selection histogram pings not equal to one", 1,
                RecordHistogram.getHistogramValueCountForTesting("ContextMenu.LensChip.Event",
                        RevampedContextMenuChipController.ChipEvent.CLICKED));
        Assert.assertFalse("Chip popoup still showing.",
                menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
    }

    // Assert that focus is unchanged and that the chip popup does not block the dismissal of the
    // context menu.
    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP})
    public void testDismissContextMenuOnClickShoppingLensChipEnabled() throws TimeoutException {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> menuCoordinator.simulateShoppyImageClassificationForTesting());
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(() -> {
            return !mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Context menu did not have window focus");

        TestTouchUtils.singleClickView(InstrumentationRegistry.getInstrumentation(), tab.getView(),
                tab.getView().getWidth() - 5, tab.getView().getHeight() - 5);

        CriteriaHelper.pollUiThread(() -> {
            return mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Activity did not regain focus.");
    }

    @Test
    @MediumTest
    public void testDismissContextMenuOnClick() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(() -> {
            return !mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Context menu did not have window focus");

        TestTouchUtils.singleClickView(InstrumentationRegistry.getInstrumentation(), tab.getView(),
                tab.getView().getWidth() - 5, tab.getView().getHeight() - 5);

        CriteriaHelper.pollUiThread(() -> {
            return mDownloadTestRule.getActivity().hasWindowFocus();
        }, "Activity did not regain focus.");
    }

    @Test
    @MediumTest
    public void testCopyEmailAddress() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                        CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            RevampedContextMenuUtils.selectContextMenuItem(
                    InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(),
                    tab, "testEmail", R.id.contextmenu_copy);
        }

        Assert.assertEquals("Copied email address is not correct",
                "someone1@example.com,someone2@example.com", getClipboardText());
    }

    @Test
    @MediumTest
    public void testCopyTelNumber() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Allow DiskWrites temporarily in main thread to avoid
        // violation during copying under emulator environment.
        try (CloseableOnMainThread ignored = CloseableOnMainThread.StrictMode.allowDiskWrites()) {
            RevampedContextMenuUtils.selectContextMenuItem(
                    InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(),
                    tab, "testTel", R.id.contextmenu_copy);
        }

        Assert.assertEquals("Copied tel number is not correct", "10000000000", getClipboardText());
    }

    @Test
    @LargeTest
    public void testSaveDataUrl() throws TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("dataUrlIcon", R.id.contextmenu_save_image, FILENAME_GIF);
    }

    @Test
    @LargeTest
    public void testSaveImage() throws TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("testImage", R.id.contextmenu_save_image, FILENAME_PNG);
    }

    @Test
    @LargeTest
    public void testSaveVideo() throws TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("videoDOMElement", R.id.contextmenu_save_video, FILENAME_WEBM);
    }

    /**
     * Opens a link and image in new tabs and verifies the order of the tabs. Also verifies that
     * the parent page remains in front after opening links in new tabs.
     *
     * This test only applies in tabbed mode. In document mode, Android handles the ordering of the
     * tabs.
     */
    @Test
    @LargeTest
    public void testOpenLinksInNewTabsAndVerifyTabIndexOrdering() throws TimeoutException {
        TabModel tabModel = mDownloadTestRule.getActivity().getCurrentTabModel();
        int numOpenedTabs = tabModel.getCount();
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink", R.id.contextmenu_open_in_new_tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int indexOfLinkPage = numOpenedTabs;
        numOpenedTabs += 1;
        Assert.assertEquals(
                "Number of open tabs does not match", numOpenedTabs, tabModel.getCount());

        // Wait for any new tab animation to finish if we're being driven by the compositor.
        final LayoutManagerImpl layoutDriver =
                mDownloadTestRule.getActivity().getCompositorViewHolder().getLayoutManager();
        CriteriaHelper.pollUiThread(() -> {
            return layoutDriver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Background tab animation not finished.");

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink2",
                R.id.contextmenu_open_in_new_tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int indexOfLinkPage2 = numOpenedTabs;
        numOpenedTabs += 1;
        Assert.assertEquals(
                "Number of open tabs does not match", numOpenedTabs, tabModel.getCount());

        // Verify the Url is still the same of Parent page.
        Assert.assertEquals(mTestUrl,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mDownloadTestRule.getActivity().getActivityTab()));

        // Verify that the background tabs were opened in the expected order.
        String newTabUrl =
                mTestServer.getURL("/chrome/test/data/android/contextmenu/test_link.html");
        Assert.assertEquals(newTabUrl,
                ChromeTabUtils.getUrlStringOnUiThread(tabModel.getTabAt(indexOfLinkPage)));

        String imageUrl =
                mTestServer.getURL("/chrome/test/data/android/contextmenu/test_link2.html");
        Assert.assertEquals(imageUrl,
                ChromeTabUtils.getUrlStringOnUiThread(tabModel.getTabAt(indexOfLinkPage2)));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testContextMenuRetrievesLinkOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_save_link_as,
                R.id.contextmenu_copy_link_text, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_share_link};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_in_ephemeral_tab});
        expectedItems = addItemsIf(ChromeFeatureList.isEnabled(ChromeFeatureList.READ_LATER),
                expectedItems, new Integer[] {R.id.contextmenu_read_later});
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    public void testContextMenuRetrievesImageOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_by_image,
                R.id.contextmenu_share_image, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    public void testContextMenuRetrievesImageOptionsWithLensShoppingAllowlist()
            throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_by_image,
                R.id.contextmenu_share_image, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testContextMenuRetrievesImageOptionsLensEnabled() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_with_google_lens,
                R.id.contextmenu_share_image, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/ShopSimilarProducts"})
    public void
    testContextMenuLensEnabledShopSimilarProducts() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_shop_similar_products, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/ShopImageWithGoogleLens"})
    public void
    testContextMenuLensEnabledShopImageWithGoogleLens() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_shop_image_with_google_lens, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:"
                    + "lensShopVariation/SearchSimilarProducts"})
    public void
    testContextMenuLensEnabledSeachSimilarProducts() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_search_similar_products, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:shoppingUrlPatterns/^shopping-site.*"})
    public void
    testContextMenuLensDisableShopWithGoogleLensForShoppingUrl() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_with_google_lens,
                R.id.contextmenu_share_image, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST,
            ChromeFeatureList.CONTEXT_MENU_SEARCH_AND_SHOP_WITH_GOOGLE_LENS})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_SHOP_WITH_GOOGLE_LENS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:lensShopVariation/ShopSimilarProducts"})
    public void
    testContextMenuLensEnabledSearchAndShopSimilarProducts() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        LensUtils.setFakeImageUrlInShoppingAllowlistForTesting(true);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_with_google_lens,
                R.id.contextmenu_share_image, R.id.contextmenu_shop_similar_products,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    public void testContextMenuRetrievesImageOptions_NoDefaultSearchEngine()
            throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    public void testContextMenuRetrievesImageOptions_NoDefaultSearchEngineLensEnabled()
            throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");

        // Search with Google Lens is only supported when Google is the default search provider.
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    public void testContextMenuRetrievesImageLinkOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_save_link_as, R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_by_image,
                R.id.contextmenu_share_image, R.id.contextmenu_share_link,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_in_ephemeral_tab,
                R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testContextMenuRetrievesImageLinkOptionsSearchLensEnabled()
            throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_save_link_as, R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_search_with_google_lens,
                R.id.contextmenu_share_image, R.id.contextmenu_share_link,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_in_ephemeral_tab,
                R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:orderShareImageBeforeLens/true"})
    public void
    testContextMenuShareImageStillAddedWhenReordered() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab,
                R.id.contextmenu_open_in_incognito_tab, R.id.contextmenu_copy_link_address,
                R.id.contextmenu_save_link_as, R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_search_with_google_lens, R.id.contextmenu_share_link,
                R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_in_ephemeral_tab,
                R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testContextMenuRetrievesVideoOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        DOMUtils.clickNode(
                mDownloadTestRule.getActivity().getCurrentWebContents(), "videoDOMElement");
        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "videoDOMElement");

        Integer[] expectedItems = {R.id.contextmenu_save_video};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled"})
    public void
    testSearchWithGoogleLensMenuItemName() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_search_with_google_lens};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_in_ephemeral_tab});
        String title = getMenuTitleFromItem(menu, R.id.contextmenu_search_with_google_lens);
        Assert.assertTrue("Context menu item name should be \'Search with Google Lens\'.",
                title.startsWith("Search with Google Lens"));
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useSearchImageWithGoogleLensItemName/true"})
    public void
    testSearchImageWithGoogleLensMenuItemName() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        ShareHelper.setIgnoreActivityNotFoundExceptionForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        RevampedContextMenuCoordinator menu =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_search_with_google_lens};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_in_ephemeral_tab});
        String title = getMenuTitleFromItem(menu, R.id.contextmenu_search_with_google_lens);
        Assert.assertTrue("Context menu item name should be \'Search image with Google Lens\'.",
                title.startsWith("Search image with Google Lens"));
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testCopyImage() throws Throwable {
        // Clear the clipboard.
        Clipboard.getInstance().setText("");

        hardcodeTestImageForSharing(TEST_GIF_IMAGE_FILE_EXTENSION);
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                        CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            RevampedContextMenuUtils.selectContextMenuItem(
                    InstrumentationRegistry.getInstrumentation(), mDownloadTestRule.getActivity(),
                    tab, "dataUrlIcon", R.id.contextmenu_copy_image);
        }

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(Clipboard.getInstance().getImageUri(), Matchers.notNullValue());
        });

        String imageUriString = Clipboard.getInstance().getImageUri().toString();

        Assert.assertTrue("Image content prefix is not correct",
                imageUriString.startsWith(
                        "content://org.chromium.chrome.tests.FileProvider/images/screenshot/"));
        Assert.assertTrue("Image extension is not correct",
                imageUriString.endsWith(TEST_GIF_IMAGE_FILE_EXTENSION));

        // Clean up the clipboard.
        Clipboard.getInstance().setText("");
    }

    /**
     * Takes all the visible items on the menu and compares them to a the list of expected items.
     * @param menu A context menu that is displaying visible items.
     * @param expectedItems A list of items that is expected to appear within a context menu. The
     *                      list does not need to be ordered.
     */
    private void assertMenuItemsAreEqual(
            RevampedContextMenuCoordinator menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                actualItems.add(
                        menu.getItem(i).model.get(RevampedContextMenuItemProperties.MENU_ID));
            }
        }

        Assert.assertThat("Populated menu items were:" + getMenuTitles(menu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private String getMenuTitles(RevampedContextMenuCoordinator menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                items.append("\n").append(
                        menu.getItem(i).model.get(RevampedContextMenuItemProperties.TEXT));
            }
        }
        return items.toString();
    }

    private String getMenuTitleFromItem(RevampedContextMenuCoordinator menu, int itemId) {
        StringBuilder itemName = new StringBuilder();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                if (menu.getItem(i).model.get(RevampedContextMenuItemProperties.MENU_ID)
                        == itemId) {
                    itemName.append(
                            menu.getItem(i).model.get(RevampedContextMenuItemProperties.TEXT));
                    return itemName.toString();
                }
            }
        }
        return null;
    }

    /**
     * Adds items to the baseItems if the given condition is true.
     * @param condition The condition to check for whether to add items or not.
     * @param baseItems The base list of items to add to.
     * @param additionalItems The additional items to add.
     * @return An array of items that has the additional items added if the condition is true.
     */
    private Integer[] addItemsIf(
            boolean condition, Integer[] baseItems, Integer[] additionalItems) {
        List<Integer> variableItems = new ArrayList<>();
        variableItems.addAll(Arrays.asList(baseItems));
        if (condition) {
            for (int i = 0; i < additionalItems.length; i++) variableItems.add(additionalItems[i]);
        }
        return variableItems.toArray(baseItems);
    }

    private void saveMediaFromContextMenu(String mediaDOMElement, int saveMenuID,
            String expectedFilename) throws TimeoutException, SecurityException, IOException {
        // Select "save [image/video]" in that menu.
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, mediaDOMElement, saveMenuID);

        // Wait for the download to complete and see if we got the right file
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        mDownloadTestRule.checkLastDownload(expectedFilename);
    }

    private String getClipboardText() throws Throwable {
        final AtomicReference<String> clipboardTextRef = new AtomicReference<>();
        mDownloadTestRule.runOnUiThread(() -> {
            ClipboardManager clipMgr =
                    (ClipboardManager) mDownloadTestRule.getActivity().getSystemService(
                            Context.CLIPBOARD_SERVICE);
            ClipData clipData = clipMgr.getPrimaryClip();
            Assert.assertNotNull("Primary clip is null", clipData);
            Assert.assertTrue("Primary clip contains no items.", clipData.getItemCount() > 0);
            clipboardTextRef.set(clipData.getItemAt(0).getText().toString());
        });
        return clipboardTextRef.get();
    }

    /**
     * Hardcode image bytes to non-null arbitrary data.
     * @param extension Image file extension.
     */
    private void hardcodeTestImageForSharing(String extension) {
        // This string just needs to be not empty in order for the code to accept it as valid
        // image data and generate the temp file for sharing. In the future we could explore
        // transcoding the actual test image from png to jpeg to make the test more realistic.
        String mockImageData = "randomdata";
        byte[] mockImageByteArray = mockImageData.getBytes();
        // See function javadoc for more context.
        ContextMenuNativeDelegateImpl.setHardcodedImageBytesForTesting(
                mockImageByteArray, extension);
    }

    private void assertStringContains(String subString, String superString) {
        Assert.assertTrue("String '" + superString + "' does not contain '" + subString + "'",
                superString.contains(subString));
    }

    /**
     * Makes sure there are no files with names identical to the ones this test uses in the
     * downloads directory
     */
    private void deleteTestFiles() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
