// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Looper;
import android.text.TextUtils;
import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for the context menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.GOOGLE_BASE_URL + "=http://example.com/"})
public class ContextMenuTest implements DownloadTestRule.CustomMainActivityStart {

    @Mock
    private ContextMenuItemDelegate mItemDelegate;
    @Mock
    private ShareDelegate mShareDelegate;

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
    // Test Lens chip delegate that always returns valid chip render params.
    private void setupLensChipDelegate() {
        LensChipDelegate.setShouldSkipIsEnabledCheckForTesting(true);
    }

    private static final String[] TEST_FILES =
            new String[] {FILENAME_GIF, FILENAME_PNG, FILENAME_WEBM};

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        setupLensChipDelegate();
    }

    @Override
    public void customMainActivityStart() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);
        deleteTestFiles();
        mDownloadTestRule.startMainActivityWithURL(mTestUrl);
        mDownloadTestRule.assertWaitForPageScaleFactorMatch(0.5f);
    }

    @After
    public void tearDown() {
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
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "testLink",
                    R.id.contextmenu_copy_link_address);
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
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "testImageLink",
                    R.id.contextmenu_copy_link_address);
        }

        assertStringContains("test_link.html", getClipboardText());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testLongPressOnImage() throws TimeoutException {
        checkOpenImageInNewTab("testImage", "/chrome/test/data/android/contextmenu/test_image.png");
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    public void testLongPressOnImageLink() throws TimeoutException {
        checkOpenImageInNewTab(
                "testImageLink", "/chrome/test/data/android/contextmenu/test_image.png");
    }

    private void checkOpenImageInNewTab(String domId, final String expectedPath)
            throws TimeoutException {
        final Tab activityTab = mDownloadTestRule.getActivity().getActivityTab();

        final CallbackHelper newTabCallback = new CallbackHelper();
        final AtomicReference<Tab> newTab = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDownloadTestRule.getActivity().getTabModelSelector().addObserver(
                    new TabModelSelectorObserver() {
                        @Override
                        public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                            if (tab.getParentId() != activityTab.getId()) {
                                return;
                            }
                            newTab.set(tab);
                            newTabCallback.notifyCalled();

                            mDownloadTestRule.getActivity().getTabModelSelector().removeObserver(
                                    this);
                        }
                    });
        });

        int callbackCount = newTabCallback.getCallCount();

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
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
        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
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
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testLensTranslateChipNotShowingIfNotEnabled() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Chip popoup was initialized.",
                    menuCoordinator.getCurrentPopupWindowForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testSelectLensTranslateChip() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            menuCoordinator.simulateTranslateImageClassificationForTesting();
            Assert.assertTrue("Chip popoup not showing.",
                    menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
            menuCoordinator.clickChipForTesting();
        });

        Assert.assertFalse("Chip popoup still showing.",
                menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testLensChipNotShowingAfterMenuDismissed() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
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
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testDismissContextMenuOnClickLensTranslateChipEnabled() throws TimeoutException {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
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
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testLensShoppingChipNotShowingIfNotEnabled() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Chip popoup was initialized.",
                    menuCoordinator.getCurrentPopupWindowForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"Browser"})
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testSelectLensShoppingChip() throws Throwable {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
        // Needs to run on UI thread so creation happens on same thread as dismissal.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            menuCoordinator.simulateShoppyImageClassificationForTesting();
            Assert.assertTrue("Chip popoup not showing.",
                    menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
            menuCoordinator.clickChipForTesting();
        });

        Assert.assertFalse("Chip popoup still showing.",
                menuCoordinator.getCurrentPopupWindowForTesting().isShowing());
    }

    // Assert that focus is unchanged and that the chip popup does not block the dismissal of the
    // context menu.
    @Test
    @MediumTest
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testDismissContextMenuOnClickShoppingLensChipEnabled() throws TimeoutException {
        // Required to avoid runtime error.
        Looper.prepare();

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
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
        ContextMenuCoordinator menuCoordinator = ContextMenuUtils.openContextMenu(tab, "testImage");
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
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "testEmail", R.id.contextmenu_copy);
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
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "testTel", R.id.contextmenu_copy);
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
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink", R.id.contextmenu_open_in_new_tab);
        int indexOfLinkPage = numOpenedTabs;
        final int expectedNumOpenedTabs = indexOfLinkPage + 1;
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Number of open tabs does not match", tabModel.getCount(),
                    Matchers.is(expectedNumOpenedTabs));
        });
        numOpenedTabs = expectedNumOpenedTabs;

        // Wait for any new tab animation to finish if we're being driven by the compositor.
        final LayoutManagerImpl layoutDriver = mDownloadTestRule.getActivity()
                                                       .getCompositorViewHolderForTesting()
                                                       .getLayoutManager();
        CriteriaHelper.pollUiThread(() -> {
            return layoutDriver.getActiveLayout().shouldDisplayContentOverlay();
        }, "Background tab animation not finished.");

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink2",
                R.id.contextmenu_open_in_new_tab);
        int indexOfLinkPage2 = numOpenedTabs;
        final int expectedNumOpenedTabs2 = indexOfLinkPage2 + 1;
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Number of open tabs does not match", tabModel.getCount(),
                    Matchers.is(expectedNumOpenedTabs2));
        });
        numOpenedTabs = expectedNumOpenedTabs2;

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
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab_in_group,
                R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_save_link_as, R.id.contextmenu_copy_link_text,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_share_link,
                R.id.contextmenu_read_later};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_in_ephemeral_tab});
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    public void testContextMenuRetrievesImageOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");

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
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_ENABLE_LENS_SHOPPING_ALLOWLIST})
    public void testContextMenuRetrievesImageOptionsWithLensShoppingAllowlist()
            throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");

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
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testContextMenuRetrievesImageOptionsLensEnabled() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");

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
    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    public void testContextMenuRetrievesImageOptions_NoDefaultSearchEngine()
            throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");

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
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");

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
    @DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    public void testContextMenuRetrievesImageLinkOptions() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab_in_group,
                R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_save_image, R.id.contextmenu_open_image_in_new_tab,
                R.id.contextmenu_search_by_image, R.id.contextmenu_share_image,
                R.id.contextmenu_share_link, R.id.contextmenu_copy_image};
        Integer[] featureItems = {R.id.contextmenu_open_in_ephemeral_tab,
                R.id.contextmenu_open_image_in_ephemeral_tab};
        expectedItems =
                addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems, featureItems);
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testContextMenuRetrievesImageLinkOptionsSearchLensEnabled()
            throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab_in_group,
                R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_save_image, R.id.contextmenu_open_image_in_new_tab,
                R.id.contextmenu_search_with_google_lens, R.id.contextmenu_share_image,
                R.id.contextmenu_share_link, R.id.contextmenu_copy_image};
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
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void
    testContextMenuShareImageStillAddedWhenReordered() throws TimeoutException {
        LensUtils.setFakePassableLensEnvironmentForTesting(true);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImageLink");

        Integer[] expectedItems = {R.id.contextmenu_open_in_new_tab_in_group,
                R.id.contextmenu_open_in_new_tab, R.id.contextmenu_open_in_incognito_tab,
                R.id.contextmenu_copy_link_address, R.id.contextmenu_save_link_as,
                R.id.contextmenu_save_image, R.id.contextmenu_open_image_in_new_tab,
                R.id.contextmenu_share_image, R.id.contextmenu_search_with_google_lens,
                R.id.contextmenu_share_link, R.id.contextmenu_copy_image};
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
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "videoDOMElement");

        Integer[] expectedItems = {R.id.contextmenu_save_video};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @EnableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testSearchImageWithGoogleLensMenuItemName() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_search_with_google_lens};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_image_in_ephemeral_tab});
        String title = getMenuTitleFromItem(menu, R.id.contextmenu_search_with_google_lens);
        Assert.assertTrue("Context menu item name should be \'Search image with Google\'.",
                title.startsWith("Search image with Google"));
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_SEARCH_OPTIMIZATIONS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useLensContextMenuAlternateText1/true"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void
    testSearchImageWithGoogleLensAltText1MenuItemName() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_search_with_google_lens};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_image_in_ephemeral_tab});
        String title = getMenuTitleFromItem(menu, R.id.contextmenu_search_with_google_lens);
        Assert.assertTrue("Context menu item name should be \'Search image with Google\'.",
                title.startsWith("Search image with Google"));
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_SEARCH_OPTIMIZATIONS
                    + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:useLensContextMenuAlternateText2/true"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void
    testSearchImageWithGoogleLensAltText2MenuItemName() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        LensUtils.setFakePassableLensEnvironmentForTesting(true);
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(tab, "testImage");
        Integer[] expectedItems = {R.id.contextmenu_save_image,
                R.id.contextmenu_open_image_in_new_tab, R.id.contextmenu_share_image,
                R.id.contextmenu_copy_image, R.id.contextmenu_search_with_google_lens};
        expectedItems = addItemsIf(EphemeralTabCoordinator.isSupported(), expectedItems,
                new Integer[] {R.id.contextmenu_open_image_in_ephemeral_tab});
        String title = getMenuTitleFromItem(menu, R.id.contextmenu_search_with_google_lens);
        Assert.assertTrue("Context menu item name should be \'Search inside image with Google\'.",
                title.startsWith("Search inside image with Google"));
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
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "dataUrlIcon",
                    R.id.contextmenu_copy_image);
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

    @Test
    @SmallTest
    @Feature({"Browser", "ContextMenu"})
    public void testContextMenuOpenedFromHighlight() {
        when(mItemDelegate.isIncognito()).thenReturn(false);
        when(mItemDelegate.getPageTitle()).thenReturn("");

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuHelper contextMenuHelper =
                ContextMenuHelper.createForTesting(0, tab.getWebContents());
        ContextMenuParams params = new ContextMenuParams(0, 0, new GURL("http://example.com/"),
                GURL.emptyGURL(), "", GURL.emptyGURL(), GURL.emptyGURL(), "", null, false, 0, 0,
                MenuSourceType.MENU_SOURCE_TOUCH, /*getOpenedFromHighlight=*/true,
                /*additionalNavigationParams=*/null);
        ContextMenuPopulatorFactory populatorFactory = new ChromeContextMenuPopulatorFactory(
                mItemDelegate,
                ()
                        -> mShareDelegate,
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL, ExternalAuthUtils.getInstance());
        Integer[] expectedItems = {R.id.contextmenu_share_highlight,
                R.id.contextmenu_remove_highlight, R.id.contextmenu_learn_more};
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ContextMenuHelper.setMenuShownCallbackForTests((coordinator) -> {
                assertMenuItemsAreEqual(coordinator, expectedItems);
                Assert.assertEquals(
                        1, RecordHistogram.getHistogramTotalCountForTesting("ContextMenu.Shown"));
                Assert.assertEquals(1,
                        RecordHistogram.getHistogramTotalCountForTesting(
                                "ContextMenu.Shown.SharedHighlightingInteraction"));
            });
            contextMenuHelper.showContextMenuForTesting(
                    populatorFactory, params, null, tab.getView(), 0);
        });
    }

    @Test
    @SmallTest
    public void testShareImage() throws Exception {
        hardcodeTestImageForSharing(TEST_JPG_IMAGE_FILE_EXTENSION);

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        // Set share delegate before triggering context menu, so the mocked share delegate is used.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            var supplier = (ShareDelegateSupplier) ShareDelegateSupplier.from(
                    mDownloadTestRule.getActivity().getWindowAndroid());
            Mockito.doReturn(true).when(mShareDelegate).isSharingHubEnabled();
            supplier.set(mShareDelegate);
        });

        // Allow all thread policies temporarily in main thread to avoid
        // DiskWrite and UnBufferedIo violations during copying under
        // emulator environment.
        try (CloseableOnMainThread ignored =
                        CloseableOnMainThread.StrictMode.allowAllThreadPolicies()) {
            ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                    mDownloadTestRule.getActivity(), tab, "testImage",
                    R.id.contextmenu_share_image);
        }

        ArgumentCaptor<ShareParams> shareParamsCaptor = ArgumentCaptor.forClass(ShareParams.class);
        ArgumentCaptor<ChromeShareExtras> chromeExtrasCaptor =
                ArgumentCaptor.forClass(ChromeShareExtras.class);
        verify(mShareDelegate)
                .share(shareParamsCaptor.capture(), chromeExtrasCaptor.capture(),
                        eq(ShareOrigin.CONTEXT_MENU));

        Assert.assertTrue("Content being shared is expected to be image.",
                shareParamsCaptor.getValue().getFileContentType().startsWith("image"));
        Assert.assertTrue("Share with share sheet expect to record the last used.",
                chromeExtrasCaptor.getValue().saveLastUsed());
    }

    @Test
    @SmallTest
    public void testShareLink() throws Exception {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();

        // Set share delegate before triggering context menu, so the mocked share delegate is used.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            var supplier = (ShareDelegateSupplier) ShareDelegateSupplier.from(
                    mDownloadTestRule.getActivity().getWindowAndroid());
            supplier.set(mShareDelegate);
        });
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testImage", R.id.contextmenu_share_link);

        verify(mShareDelegate).share(any(), any(), eq(ShareOrigin.CONTEXT_MENU));

        ArgumentCaptor<ShareParams> shareParamsCaptor = ArgumentCaptor.forClass(ShareParams.class);
        ArgumentCaptor<ChromeShareExtras> chromeExtrasCaptor =
                ArgumentCaptor.forClass(ChromeShareExtras.class);
        verify(mShareDelegate)
                .share(shareParamsCaptor.capture(), chromeExtrasCaptor.capture(),
                        eq(ShareOrigin.CONTEXT_MENU));

        Assert.assertFalse("Link being shared is empty.",
                TextUtils.isEmpty(shareParamsCaptor.getValue().getUrl()));
        Assert.assertTrue("Share with share sheet expect to record the last used.",
                chromeExtrasCaptor.getValue().saveLastUsed());
    }

    // TODO(benwgold): Add more test coverage for histogram recording of other context menu types.

    /**
     * Takes all the visible items on the menu and compares them to a the list of expected items.
     * @param menu A context menu that is displaying visible items.
     * @param expectedItems A list of items that is expected to appear within a context menu. The
     *                      list does not need to be ordered.
     */
    private void assertMenuItemsAreEqual(ContextMenuCoordinator menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                actualItems.add(menu.getItem(i).model.get(ContextMenuItemProperties.MENU_ID));
            }
        }

        assertThat("Populated menu items were:" + getMenuTitles(menu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private String getMenuTitles(ContextMenuCoordinator menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                items.append("\n").append(
                        menu.getItem(i).model.get(ContextMenuItemProperties.TEXT));
            }
        }
        return items.toString();
    }

    private String getMenuTitleFromItem(ContextMenuCoordinator menu, int itemId) {
        StringBuilder itemName = new StringBuilder();
        for (int i = 0; i < menu.getCount(); i++) {
            if (menu.getItem(i).type >= CONTEXT_MENU_ITEM) {
                if (menu.getItem(i).model.get(ContextMenuItemProperties.MENU_ID) == itemId) {
                    itemName.append(menu.getItem(i).model.get(ContextMenuItemProperties.TEXT));
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
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, mediaDOMElement, saveMenuID);

        // Wait for the download to complete and see if we got the right file
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownloaded(expectedFilename, null));
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
