// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Environment;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.util.Pair;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests Chrome download feature by attempting to download some files.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations
        .UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
        @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        public class DownloadTest implements CustomMainActivityStart {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = Arrays.asList(
            new ParameterSet().value(true).name("UseDownloadOfflineContentProviderEnabled"),
            new ParameterSet().value(false).name("UseDownloadOfflineContentProviderDisabled"));

    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String TAG = "DownloadTest";
    private static final String SUPERBO_CONTENTS =
            "plain text response from a POST";

    private EmbeddedTestServer mTestServer;

    private static final String TEST_DOWNLOAD_DIRECTORY = "/chrome/test/data/android/download/";

    private static final String FILENAME_WALLPAPER = "[large]wallpaper.dm";
    private static final String FILENAME_TEXT = "superbo.txt";
    private static final String FILENAME_TEXT_1 = "superbo (1).txt";
    private static final String FILENAME_TEXT_2 = "superbo (2).txt";
    private static final String FILENAME_SWF = "test.swf";
    private static final String FILENAME_GZIP = "test.gzip";

    private static final String[] TEST_FILES = new String[] {
        FILENAME_WALLPAPER, FILENAME_TEXT, FILENAME_TEXT_1, FILENAME_TEXT_2, FILENAME_SWF,
        FILENAME_GZIP
    };

    private boolean mUseDownloadOfflineContentProvider;

    static class DownloadManagerRequestInterceptorForTest
            implements DownloadManagerService.DownloadManagerRequestInterceptor {
        public DownloadItem mDownloadItem;

        @Override
        public void interceptDownloadRequest(DownloadItem item, boolean notifyComplete) {
            mDownloadItem = item;
            Assert.assertTrue(notifyComplete);
        }
    }

    public DownloadTest(boolean useDownloadOfflineContentProvider) {
        mUseDownloadOfflineContentProvider = useDownloadOfflineContentProvider;
    }

    @Before
    public void setUp() {
        deleteTestFiles();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        deleteTestFiles();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        if (mUseDownloadOfflineContentProvider) {
            Features.getInstance().enable(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER);
        } else {
            Features.getInstance().disable(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER);
        }
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @RetryOnFailure
    public void testHttpGetDownload() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownload(FILENAME_GZIP, null));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @RetryOnFailure
    public void testDangerousDownload() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "dangerous.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(1);
        Assert.assertTrue("OK button wasn't found",
                InfoBarUtil.clickPrimaryButton(mDownloadTestRule.getInfoBars().get(0)));
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownload(FILENAME_SWF, null));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @RetryOnFailure
    public void testHttpPostDownload() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownload(FILENAME_TEXT, SUPERBO_CONTENTS));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/286315")
    public void testCloseEmptyDownloadTab() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html"));
        waitForFocus();
        final int initialTabCount = mDownloadTestRule.getActivity().getCurrentTabModel().getCount();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.longPressView(currentView);

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mDownloadTestRule.getActivity(), R.id.contextmenu_open_in_new_tab, 0);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownload(FILENAME_GZIP, null));

        CriteriaHelper.pollUiThread(
                Criteria.equals(initialTabCount,
                        () -> mDownloadTestRule.getActivity().getCurrentTabModel().getCount()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/849876")
    @Feature({"Downloads"})
    @RetryOnFailure
    public void testDuplicateHttpPostDownload_Download() throws Exception {
        // Snackbar overlaps the infobar which is clicked in this test.
        mDownloadTestRule.getActivity().getSnackbarManager().disableForTesting();
        // Download a file.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue("Failed to finish downloading file for the first time.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(1);
        Assert.assertTrue("Download button wasn't found",
                InfoBarUtil.clickPrimaryButton(mDownloadTestRule.getInfoBars().get(0)));
        Assert.assertTrue("Failed to finish downloading file for the second time.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        Assert.assertTrue("Missing first download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT, SUPERBO_CONTENTS));
        Assert.assertTrue("Missing second download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT_1, SUPERBO_CONTENTS));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/597230")
    public void testDuplicateHttpPostDownload_Cancel() {
        // Download a file.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue("Failed to finish downloading file for the first time.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(1);
        Assert.assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(mDownloadTestRule.getInfoBars().get(0)));
        Assert.assertFalse("Download should not happen when clicking cancel button",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @FlakyTest(message = "crbug.com/415711")
    public void testDuplicateHttpPostDownload_Dismiss() throws Exception {
        // Download a file.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue("Failed to finish downloading file for the first time.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(1);
        Assert.assertTrue("Close button wasn't found",
                InfoBarUtil.clickCloseButton(mDownloadTestRule.getInfoBars().get(0)));
        Assert.assertFalse("Download should not happen when closing infobar",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        Assert.assertTrue("Missing first download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT, SUPERBO_CONTENTS));
        Assert.assertFalse("Should not have second download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT_1, SUPERBO_CONTENTS));
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/597230")
    public void testDuplicateHttpPostDownload_AllowMultipleInfoBars() throws Exception {
        Assert.assertFalse(mDownloadTestRule.hasDownload(FILENAME_TEXT, SUPERBO_CONTENTS));
        // Download a file.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue("Failed to finish downloading file for the first time.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Download the file for the second time.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(1);

        // Download the file for the third time.
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "post.html"));
        waitForFocus();
        currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        TouchCommon.singleClickView(currentView);
        assertPollForInfoBarSize(2);

        // Now create two new files by clicking on the infobars.
        callCount = mDownloadTestRule.getChromeDownloadCallCount();
        Assert.assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(mDownloadTestRule.getInfoBars().get(0)));
        Assert.assertTrue("Failed to finish downloading the second file.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        assertPollForInfoBarSize(1);
        callCount = mDownloadTestRule.getChromeDownloadCallCount();
        Assert.assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(mDownloadTestRule.getInfoBars().get(0)));
        Assert.assertTrue("Failed to finish downloading the third file.",
                mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        Assert.assertTrue("Missing first download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT, SUPERBO_CONTENTS));
        Assert.assertTrue("Missing second download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT_1, SUPERBO_CONTENTS));
        Assert.assertTrue("Missing third download",
                mDownloadTestRule.hasDownload(FILENAME_TEXT_2, SUPERBO_CONTENTS));
    }

    private void goToLastTab() {
        final TabModel model = mDownloadTestRule.getActivity().getCurrentTabModel();
        final int count = model.getCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> TabModelUtils.setIndex(model, count - 1));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Tab tab = mDownloadTestRule.getActivity().getActivityTab();
                return tab == model.getTabAt(count - 1) && ChromeTabUtils.isRendererReady(tab);
            }
        });
    }

    private void waitForNewTabToStabilize(final int numTabsAfterNewTab) {
        // Wait until we have a new tab first. This should be called before checking the active
        // layout because the active layout changes StaticLayout --> SimpleAnimationLayout
        // --> (tab added) --> StaticLayout.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("Actual tab count: "
                        + mDownloadTestRule.getActivity().getCurrentTabModel().getCount());
                return mDownloadTestRule.getActivity().getCurrentTabModel().getCount()
                        >= numTabsAfterNewTab;
            }
        });

        // Now wait until the new tab animation finishes. Something wonky happens
        // if we try to go to the new tab before this.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CompositorViewHolder compositorViewHolder =
                        mDownloadTestRule.getActivity().findViewById(
                                R.id.compositor_view_holder);
                LayoutManager layoutManager = compositorViewHolder.getLayoutManager();

                return layoutManager.getActiveLayout() instanceof StaticLayout;
            }
        });
    }

    @Test
    @DisabledTest(message = "crbug.com/606798")
    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_OpenNewTabAndReplace() throws Exception {
        final String url =
                mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "get.html");

        // Create the file in advance so that duplicate download infobar can show up.
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        Assert.assertTrue(dir.isDirectory());
        final File file = new File(dir, FILENAME_GZIP);
        try {
            if (!file.exists()) {
                Assert.assertTrue(file.createNewFile());
            }

            // Open in a new tab again.
            mDownloadTestRule.loadUrl(url);
            waitForFocus();

            View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
            TouchCommon.longPressView(currentView);
            InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                    mDownloadTestRule.getActivity(), R.id.contextmenu_open_in_new_tab, 0);
            waitForNewTabToStabilize(2);

            goToLastTab();
            assertPollForInfoBarSize(1);

            // Now create two new files by clicking on the infobars.
            Assert.assertTrue("OVERWRITE button wasn't found",
                    InfoBarUtil.clickPrimaryButton(mDownloadTestRule.getInfoBars().get(0)));
        } finally {
            if (!file.delete()) {
                Log.d(TAG, "Failed to delete test.gzip");
            }
        }
    }

    @Test
    @MediumTest
    @Feature({"Downloads"})
    @RetryOnFailure
    public void testUrlEscaping() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_DOWNLOAD_DIRECTORY + "urlescaping.html"));
        waitForFocus();
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        TouchCommon.singleClickView(currentView);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));
        Assert.assertTrue(mDownloadTestRule.hasDownload(FILENAME_WALLPAPER, null));
    }

    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testOMADownloadInterception() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        try {
            final DownloadManagerRequestInterceptorForTest interceptor =
                    new DownloadManagerRequestInterceptorForTest();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> DownloadManagerService.getDownloadManagerService()
                            .setDownloadManagerRequestInterceptor(interceptor));
            List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
            headers.add(Pair.create("Content-Type", "application/vnd.oma.drm.message"));
            final String url = webServer.setResponse("/test.dm", "testdata", headers);
            mDownloadTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("<script>"
                    + "  function download() {"
                    + "    window.open( '" + url + "')"
                    + "  }"
                    + "</script>"
                    + "<body id='body' onclick='download()'></body>"));
            DOMUtils.clickNode(mDownloadTestRule.getActivity().getCurrentWebContents(), "body");
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return interceptor.mDownloadItem != null
                            && url.equals(interceptor.mDownloadItem.getDownloadInfo().getUrl());
                }
            });
        } finally {
            webServer.shutdown();
        }
    }

    private void waitForFocus() {
        View currentView = mDownloadTestRule.getActivity().getActivityTab().getView();
        if (!currentView.hasFocus()) {
            TouchCommon.singleClickView(currentView);
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Wait until info bar size becomes the given size and the last info bar becomes ready if there
     * is one more more.
     * @param size The size of info bars to poll for.
     */
    private void assertPollForInfoBarSize(final int size) {
        final InfoBarContainer container = mDownloadTestRule.getInfoBarContainer();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("There should be " + size + " infobar but there are "
                        + mDownloadTestRule.getInfoBars().size() + " infobars.");
                return mDownloadTestRule.getInfoBars().size() == size && !container.isAnimating();
            }
        });
    }

    /**
     * Makes sure there are no files with names identical to the ones this test uses in the
     * downloads directory
     */
    private void deleteTestFiles() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }
}
