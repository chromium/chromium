// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.contextmenu.RevampedContextMenuUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.IOException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for the Revamped Context Menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.GOOGLE_BASE_URL + "=http://example.com/"})
@Features.EnableFeatures(ChromeFeatureList.REVAMPED_CONTEXT_MENU)
@Features.DisableFeatures({ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS})
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
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink",
                R.id.contextmenu_copy_link_address);

        assertStringContains("test_link.html", getClipboardText());
    }

    @Test
    @MediumTest
    public void testLongPressOnImage() throws TimeoutException {
        final Tab activityTab = mDownloadTestRule.getActivity().getActivityTab();

        final CallbackHelper newTabCallback = new CallbackHelper();
        final AtomicReference<Tab> newTab = new AtomicReference<>();
        mDownloadTestRule.getActivity().getTabModelSelector().addObserver(
                new EmptyTabModelSelectorObserver() {
                    @Override
                    public void onNewTabCreated(Tab tab) {
                        super.onNewTabCreated(tab);

                        if (tab.getParentId() != activityTab.getId()) return;
                        newTab.set(tab);
                        newTabCallback.notifyCalled();

                        mDownloadTestRule.getActivity().getTabModelSelector().removeObserver(this);
                    }
                });

        int callbackCount = newTabCallback.getCallCount();

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), activityTab, "testImage",
                R.id.contextmenu_open_image_in_new_tab);

        try {
            newTabCallback.waitForCallback(callbackCount);
        } catch (TimeoutException ex) {
            Assert.fail("New tab never created from context menu press");
        }

        // Only check for the URL matching as the tab will not be fully created in svelte mode.
        final String expectedUrl =
                mTestServer.getURL("/chrome/test/data/android/contextmenu/test_image.png");
        CriteriaHelper.pollUiThread(Criteria.equals(expectedUrl, () -> newTab.get().getUrl()));
    }

    @Test
    @MediumTest
    public void testDismissContextMenuOnBack() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(new Criteria("Context menu did not have window focus") {
            @Override
            public boolean isSatisfied() {
                return !mDownloadTestRule.getActivity().hasWindowFocus();
            }
        });

        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        CriteriaHelper.pollUiThread(new Criteria("Activity did not regain focus.") {
            @Override
            public boolean isSatisfied() {
                return mDownloadTestRule.getActivity().hasWindowFocus();
            }
        });
    }

    @Test
    @MediumTest
    public void testDismissContextMenuOnClick() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuCoordinator menuCoordinator =
                RevampedContextMenuUtils.openContextMenu(tab, "testImage");
        Assert.assertNotNull("Context menu was not properly created", menuCoordinator);
        CriteriaHelper.pollUiThread(new Criteria("Context menu did not have window focus") {
            @Override
            public boolean isSatisfied() {
                return !mDownloadTestRule.getActivity().hasWindowFocus();
            }
        });

        TestTouchUtils.singleClickView(InstrumentationRegistry.getInstrumentation(), tab.getView(),
                tab.getView().getWidth() - 5, tab.getView().getHeight() - 5);

        CriteriaHelper.pollUiThread(new Criteria("Activity did not regain focus.") {
            @Override
            public boolean isSatisfied() {
                return mDownloadTestRule.getActivity().hasWindowFocus();
            }
        });
    }

    @Test
    @MediumTest
    public void testCopyEmailAddress() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testEmail", R.id.contextmenu_copy);

        Assert.assertEquals("Copied email address is not correct",
                "someone1@example.com,someone2@example.com", getClipboardText());
    }

    @Test
    @MediumTest
    public void testCopyTelNumber() throws Throwable {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testTel", R.id.contextmenu_copy);

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
        final LayoutManager layoutDriver =
                mDownloadTestRule.getActivity().getCompositorViewHolder().getLayoutManager();
        CriteriaHelper.pollUiThread(new Criteria("Background tab animation not finished.") {
            @Override
            public boolean isSatisfied() {
                return layoutDriver.getActiveLayout().shouldDisplayContentOverlay();
            }
        });

        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), tab, "testLink2",
                R.id.contextmenu_open_in_new_tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int indexOfLinkPage2 = numOpenedTabs;
        numOpenedTabs += 1;
        Assert.assertEquals(
                "Number of open tabs does not match", numOpenedTabs, tabModel.getCount());

        // Verify the Url is still the same of Parent page.
        Assert.assertEquals(mTestUrl, mDownloadTestRule.getActivity().getActivityTab().getUrl());

        // Verify that the background tabs were opened in the expected order.
        String newTabUrl =
                mTestServer.getURL("/chrome/test/data/android/contextmenu/test_link.html");
        Assert.assertEquals(newTabUrl, tabModel.getTabAt(indexOfLinkPage).getUrl());

        String imageUrl =
                mTestServer.getURL("/chrome/test/data/android/contextmenu/test_link2.html");
        Assert.assertEquals(imageUrl, tabModel.getTabAt(indexOfLinkPage2).getUrl());
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
