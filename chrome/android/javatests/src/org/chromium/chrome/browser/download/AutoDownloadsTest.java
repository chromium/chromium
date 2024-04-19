// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.test.util.CloseableOnMainThread;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.ArrayList;

/** Test suite for multiple downloads permissions requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutoDownloadsTest implements CustomMainActivityStart {
    @Rule public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String TEST_FILE =
            "/content/test/data/android/auto_downloads_permissions.html";
    private EmbeddedTestServer mTestServer;

    @BeforeClass
    public static void beforeClass() {
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        mDownloadTestRule.deleteFilesInDownloadDirectory(
                new String[] {"test-image0.png", "test-image1.png"});
    }

    private void waitForDownloadDialog(ModalDialogManager manager) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(manager.isShowing(), Matchers.is(true));
                    Criteria.checkThat(
                            manager.getCurrentPresenterForTest(),
                            Matchers.is(manager.getPresenterForTest(ModalDialogType.APP)));
                });
    }

    @Test
    @MediumTest
    @Feature({"AutoDownloads"})
    public void testAutoDownloadsDialog() throws Exception {
        try (CloseableOnMainThread ignored = CloseableOnMainThread.StrictMode.allowDiskWrites()) {
            ArrayList<DirectoryOption> dirOptions = new ArrayList<>();
            dirOptions.add(
                    new DirectoryOption(
                            "Download",
                            PathUtils.getExternalStorageDirectory(),
                            1024000,
                            1024000,
                            DirectoryOption.DownloadLocationDirectoryType.DEFAULT));
            DownloadDirectoryProvider.getInstance()
                    .setDirectoryProviderForTesting(new TestDownloadDirectoryProvider(dirOptions));
        }

        mDownloadTestRule.loadUrl(mTestServer.getURL(TEST_FILE));
        ChromeActivity activity = mDownloadTestRule.getActivity();

        // Wait for "multiple downloads" permission dialog and allow.
        PermissionTestRule.waitForDialog(activity);
        PermissionTestRule.replyToDialog(PermissionTestRule.PromptDecision.ALLOW, activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mDownloadTestRule.hasDownloaded("test-image0.png", null),
                            Matchers.is(true));
                    Criteria.checkThat(
                            mDownloadTestRule.hasDownloaded("test-image1.png", null),
                            Matchers.is(true));
                });
    }
}
