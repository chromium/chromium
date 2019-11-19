// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.download.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;

/**
 * Test to verify download location change feature behaviors.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLocationChangeTest implements CustomMainActivityStart {
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private EmbeddedTestServer mTestServer;
    private static final String TEST_DATA_DIRECTORY = "/chrome/test/data/android/download/";
    private static final String TEST_FILE = "test.gzip";
    private static final long STORAGE_SIZE = 1024000;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Show the location dialog for the first time.
        promptDownloadLocationDialog(DownloadPromptStatus.SHOW_INITIAL);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // CustomMainActivityStart implementation.
    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Ensures the default download location dialog is shown to the user with SD card inserted.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Features.EnableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testDefaultDialogPositiveButtonClickThrough() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    DownloadPromptStatus.SHOW_INITIAL, DownloadUtils.getPromptForDownloadAndroid());

            simulateDownloadDirectories(true /* hasSDCard */);

            // Trigger the download through navigation.
            LoadUrlParams params =
                    new LoadUrlParams(mTestServer.getURL(TEST_DATA_DIRECTORY + TEST_FILE));
            mDownloadTestRule.getActivity().getActivityTab().loadUrl(params);
        });

        // Ensure the dialog is being shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                true, () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing()));

        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();

        // Click the button to start download.
        Espresso.onView(withId(R.id.positive_button)).perform(click());

        // Ensure download is done.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /**
     * Ensures no default download location dialog is shown to the user without SD card inserted.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Features.EnableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testNoDialogWithoutSDCard() {
        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    DownloadPromptStatus.SHOW_INITIAL, DownloadUtils.getPromptForDownloadAndroid());

            simulateDownloadDirectories(false /* hasSDCard */);

            // Trigger the download through navigation.
            LoadUrlParams params =
                    new LoadUrlParams(mTestServer.getURL(TEST_DATA_DIRECTORY + TEST_FILE));
            mDownloadTestRule.getActivity().getActivityTab().loadUrl(params);
        });

        // Ensure download is done, no download location dialog should show to interact with user.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /**
     * Provides default download directory and SD card directory.
     * @param hasSDCard Whether to simulate SD card inserted.
     */
    private void simulateDownloadDirectories(boolean hasSDCard) {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            dirs.add(buildDirectoryOption(DirectoryOption.DownloadLocationDirectoryType.DEFAULT,
                    PathUtils.getExternalStorageDirectory()));
            if (hasSDCard) {
                dirs.add(buildDirectoryOption(
                        DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL,
                        PathUtils.getDataDirectory()));
            }
        }

        DownloadDirectoryProvider.getInstance().setDirectoryProviderForTesting(
                new TestDownloadDirectoryProvider(dirs));
    }

    private DirectoryOption buildDirectoryOption(
            @DirectoryOption.DownloadLocationDirectoryType int type, String directoryPath) {
        return new DirectoryOption("Download", directoryPath, STORAGE_SIZE, STORAGE_SIZE, type);
    }

    private void promptDownloadLocationDialog(@DownloadPromptStatus int promptStatus) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { DownloadUtils.setPromptForDownloadAndroid(promptStatus); });
    }
}
