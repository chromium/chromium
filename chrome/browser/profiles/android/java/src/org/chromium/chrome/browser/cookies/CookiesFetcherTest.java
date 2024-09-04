// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import android.os.Handler;
import android.os.Looper;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ImportantFileWriterAndroid;
import org.chromium.base.ImportantFileWriterAndroidJni;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintWriter;

/** Tests for CookiesFetcher. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class CookiesFetcherTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private Profile mProfile1;
    @Mock private Profile mIncognitoProfile1;
    @Mock private ProfileProvider mProfileProvider;

    @Mock private CookiesFetcher.Natives mCookiesFetcherJni;
    @Mock private ImportantFileWriterAndroid.Natives mImportantFileWriterJni;

    private CanonicalCookie mCookie0;
    private CanonicalCookie mCookie1;
    private CanonicalCookie mCookie2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    final Handler mHandler = new Handler(Looper.getMainLooper());

                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        mHandler.postDelayed(task, delay);
                    }
                });

        Mockito.when(mProfile1.getOriginalProfile()).thenReturn(mProfile1);
        Mockito.when(mIncognitoProfile1.getOriginalProfile()).thenReturn(mProfile1);
        Mockito.when(mIncognitoProfile1.isPrimaryOTRProfile()).thenReturn(true);

        ThreadUtils.setThreadAssertsDisabledForTesting(true);

        jniMocker.mock(CookiesFetcherJni.TEST_HOOKS, mCookiesFetcherJni);
        jniMocker.mock(ImportantFileWriterAndroidJni.TEST_HOOKS, mImportantFileWriterJni);

        mCookie0 =
                new CanonicalCookie(
                        "name",
                        "value",
                        "domain",
                        "path",
                        /* creation= */ 0,
                        /* expiration= */ 1,
                        /* lastAccess= */ 0,
                        /* lastUpdate= */ 0,
                        /* secure= */ false,
                        /* httpOnly= */ true,
                        /* sameSite= */ 0,
                        /* priority= */ 0,
                        /* partitionKey= */ "",
                        /* sourceScheme= */ 1,
                        /* sourcePort= */ 72,
                        /* sourceType= */ 0);
        mCookie1 =
                new CanonicalCookie(
                        "name2",
                        "value2",
                        ".domain2",
                        "path2",
                        /* creation= */ 10,
                        /* expiration= */ 20,
                        /* lastAccess= */ 15,
                        /* lastUpdate= */ 15,
                        /* secure= */ true,
                        /* httpOnly= */ false,
                        /* sameSite= */ 1,
                        /* priority= */ 1,
                        /* partitionKey= */ "",
                        /* sourceScheme= */ 2,
                        /* sourcePort= */ 445,
                        /* sourceType= */ 1);
        mCookie2 =
                new CanonicalCookie(
                        "name3",
                        "value3",
                        "domain3",
                        "path3",
                        /* creation= */ 10,
                        /* expiration= */ 20,
                        /* lastAccess= */ 15,
                        /* lastUpdate= */ 15,
                        /* secure= */ true,
                        /* httpOnly= */ false,
                        /* sameSite= */ 2,
                        /* priority= */ 2,
                        /* partitionKey= */ "https://toplevelsite.com",
                        /* sourceScheme= */ 2,
                        /* sourcePort= */ -1,
                        /* sourceType= */ 2);
    }

    private void setupProfileProvider(Profile originalProfile, Profile incognitoProfile) {
        Mockito.when(mProfileProvider.getOriginalProfile()).thenReturn(originalProfile);
        Mockito.when(mProfileProvider.hasOffTheRecordProfile())
                .thenReturn(incognitoProfile != null);
        Mockito.when(mProfileProvider.getOffTheRecordProfile(Mockito.anyBoolean()))
                .thenReturn(incognitoProfile);
    }

    private void assertCookieFileExists(boolean exists) {
        String fileName = CookiesFetcher.fetchFileName();

        File cookieFile = new File(fileName);
        Assert.assertEquals(exists, cookieFile.exists());
    }

    @Test
    public void testRestoreCookies_NoProfile_NoExistingFile() throws Exception {
        setupProfileProvider(mProfile1, null);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider);
        assertCookieFileExists(false);
        fetcher.restoreCookies();
        ShadowLooper.idleMainLooper();
        assertCookieFileExists(false);
    }

    @Test
    public void testRestoreCookies_NoProfile_ExistingFile() throws Exception {
        setupProfileProvider(mProfile1, null);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider);

        File cookieFile = new File(CookiesFetcher.fetchFileName());
        try (PrintWriter writer = new PrintWriter(cookieFile)) {
            writer.println("Hey, I\"m a cookie!");
        }

        assertCookieFileExists(true);
        fetcher.restoreCookies();
        ShadowLooper.idleMainLooper();
        assertCookieFileExists(false);
    }

    @Test
    public void testRestoreCookies_Profile_NoExistingFile() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider);
        assertCookieFileExists(false);
        fetcher.restoreCookies();
        ShadowLooper.idleMainLooper();
        assertCookieFileExists(false);
    }

    @Test
    public void testPersistRestoreCookies() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider);

        Mockito.when(
                        mImportantFileWriterJni.writeFileAtomically(
                                Mockito.anyString(), Mockito.any(byte[].class)))
                .thenAnswer(
                        new Answer<Boolean>() {
                            @Override
                            public Boolean answer(InvocationOnMock invocation) {
                                try (FileOutputStream stream =
                                        new FileOutputStream((String) invocation.getArgument(0))) {
                                    stream.write(invocation.getArgument(1));
                                    return true;
                                } catch (Exception ex) {
                                    return false;
                                }
                            }
                        });

        fetcher.persistCookies();
        Mockito.verify(mCookiesFetcherJni).persistCookies(Mockito.eq(mIncognitoProfile1));

        CanonicalCookie[] cookies = new CanonicalCookie[3];
        cookies[0] = mCookie0;
        cookies[1] = mCookie1;
        cookies[2] = mCookie2;
        CookiesFetcher.onCookieFetchFinished(cookies);
        ShadowLooper.idleMainLooper();

        assertCookieFileExists(true);
        fetcher.restoreCookies();
        ShadowLooper.idleMainLooper();

        Mockito.verify(mCookiesFetcherJni, Mockito.times(3))
                .restoreCookies(
                        Mockito.eq(mIncognitoProfile1),
                        Mockito.anyString(),
                        Mockito.anyString(),
                        Mockito.anyString(),
                        Mockito.anyString(),
                        Mockito.anyLong(),
                        Mockito.anyLong(),
                        Mockito.anyLong(),
                        Mockito.anyLong(),
                        Mockito.anyBoolean(),
                        Mockito.anyBoolean(),
                        Mockito.anyInt(),
                        Mockito.anyInt(),
                        Mockito.anyString(),
                        Mockito.anyInt(),
                        Mockito.anyInt(),
                        Mockito.anyInt());

        assertCookieFileExists(false);
    }
}
