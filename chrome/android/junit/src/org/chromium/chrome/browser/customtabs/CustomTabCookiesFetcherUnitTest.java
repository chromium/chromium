// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.hamcrest.MatcherAssert;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.annotation.LooperMode;
import org.robolectric.util.TempDirectory;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.cookies.CookiesFetcherJni;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.ProfileProvider;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeoutException;

/** Test for {@link CustomTabCookiesFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class CustomTabCookiesFetcherUnitTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private ProfileProvider mProfileProvider;

    @Mock private CookiesFetcher.Natives mCookiesFetcherJni;

    private PausedExecutorService mExecutor = new PausedExecutorService();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);

        jniMocker.mock(CookiesFetcherJni.TEST_HOOKS, mCookiesFetcherJni);

        TempDirectory tmpDir = new TempDirectory();
        String cookieFileDir = tmpDir.create("foo").toAbsolutePath().toString();
        Mockito.when(mCookiesFetcherJni.getCookieFileDirectory(Mockito.any()))
                .thenReturn(cookieFileDir);
    }

    @Test
    public void testFilesCleanedUpPostRestore() throws IOException, TimeoutException {
        CustomTabCookiesFetcher cctCookiesFetcher =
                new CustomTabCookiesFetcher(mProfileProvider, new CipherFactory(), 7);

        File cookieDir = cctCookiesFetcher.getCookieDir();

        // Add a file that is applicable for deletion by setting the last modified time to exceed
        // the expiry threshold.
        File outOfDateCookieFile =
                new File(
                        cookieDir,
                        CustomTabCookiesFetcher.COOKIE_FILE_PREFIX
                                + "9"
                                + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        outOfDateCookieFile.createNewFile();
        Assert.assertTrue(
                outOfDateCookieFile.setLastModified(
                        System.currentTimeMillis()
                                - 2 * CustomTabFileUtils.STATE_EXPIRY_THRESHOLD));
        Assert.assertTrue(outOfDateCookieFile.exists());

        CallbackHelper restoreCallback = new CallbackHelper();
        cctCookiesFetcher.restoreCookies(restoreCallback::notifyCalled);

        mExecutor.runAll();
        Assert.assertFalse(outOfDateCookieFile.exists());
        restoreCallback.waitForOnly();
    }

    @Test
    public void testGetCctCookieFiles() throws IOException {
        MatcherAssert.assertThat(
                CustomTabCookiesFetcher.getCctCookieFiles(new File("blah")), Matchers.empty());

        TempDirectory tmpDirectory = new TempDirectory();
        File cookieDirectory = tmpDirectory.create("cookieDir").toFile();

        MatcherAssert.assertThat(
                CustomTabCookiesFetcher.getCctCookieFiles(cookieDirectory), Matchers.empty());

        File validCookieFile1 =
                new File(
                        cookieDirectory,
                        CustomTabCookiesFetcher.COOKIE_FILE_PREFIX
                                + "2"
                                + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        validCookieFile1.createNewFile();
        File validCookieFile2 =
                new File(
                        cookieDirectory,
                        CustomTabCookiesFetcher.COOKIE_FILE_PREFIX
                                + "784565"
                                + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        validCookieFile2.createNewFile();

        File invalidCookieFile1 =
                new File(cookieDirectory, CustomTabCookiesFetcher.COOKIE_FILE_PREFIX + "2");
        invalidCookieFile1.createNewFile();
        File invalidCookieFile2 =
                new File(cookieDirectory, "2" + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        invalidCookieFile2.createNewFile();
        File invalidCookieFile3 =
                new File(
                        cookieDirectory,
                        CustomTabCookiesFetcher.COOKIE_FILE_PREFIX
                                + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        invalidCookieFile3.createNewFile();
        File invalidCookieFile4 =
                new File(
                        cookieDirectory,
                        CustomTabCookiesFetcher.COOKIE_FILE_PREFIX
                                + "cow"
                                + CustomTabCookiesFetcher.COOKIE_FILE_EXTENSION);
        invalidCookieFile4.createNewFile();
        File invalidCookieFile5 =
                new File(cookieDirectory, CustomTabCookiesFetcher.COOKIE_FILE_PREFIX + "6aDAT");
        invalidCookieFile5.createNewFile();
        File invalidCookieFile6 =
                new File(cookieDirectory, CookiesFetcher.DEFAULT_COOKIE_FILE_NAME);
        invalidCookieFile6.createNewFile();

        MatcherAssert.assertThat(
                CustomTabCookiesFetcher.getCctCookieFiles(cookieDirectory),
                Matchers.containsInAnyOrder(validCookieFile1, validCookieFile2));
    }
}
