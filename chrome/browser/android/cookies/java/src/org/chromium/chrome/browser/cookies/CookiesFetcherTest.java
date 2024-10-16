// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.util.TempDirectory;

import org.chromium.base.ImportantFileWriterAndroid;
import org.chromium.base.ImportantFileWriterAndroidJni;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.test.PausedExecutorTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintWriter;

/** Tests for CookiesFetcher. */
@RunWith(BaseRobolectricTestRunner.class)
public class CookiesFetcherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public PausedExecutorTestRule mExecutorRule = new PausedExecutorTestRule();

    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private Profile mProfile1;
    @Mock private Profile mIncognitoProfile1;
    @Mock private ProfileProvider mProfileProvider;

    @Mock private CookiesFetcher.Natives mCookiesFetcherJni;
    @Mock private ImportantFileWriterAndroid.Natives mImportantFileWriterJni;

    private CipherFactory mCipherFactory;

    private CanonicalCookie mCookie0;
    private CanonicalCookie mCookie1;
    private CanonicalCookie mCookie2;

    @Before
    public void setUp() {
        Mockito.when(mProfile1.getOriginalProfile()).thenReturn(mProfile1);
        Mockito.when(mIncognitoProfile1.getOriginalProfile()).thenReturn(mProfile1);
        Mockito.when(mIncognitoProfile1.isPrimaryOTRProfile()).thenReturn(true);

        jniMocker.mock(CookiesFetcherJni.TEST_HOOKS, mCookiesFetcherJni);

        TempDirectory tmpDir = new TempDirectory();
        String cookieFileDir = tmpDir.create("foo").toAbsolutePath().toString();
        Mockito.when(mCookiesFetcherJni.getCookieFileDirectory(Mockito.any()))
                .thenReturn(cookieFileDir);

        jniMocker.mock(ImportantFileWriterAndroidJni.TEST_HOOKS, mImportantFileWriterJni);
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

        mCipherFactory = new CipherFactory();

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

    private static String fetchLegacyFileName() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        String fileName = CookiesFetcher.fetchLegacyFileName();
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(false);
        return fileName;
    }

    private static String fetchAbsoluteFilePath(CookiesFetcher fetcher) {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        String fileName = fetcher.fetchAbsoluteFilePath();
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(false);
        return fileName;
    }

    private void assertLegacyCookieFileExists(boolean exists) {
        String fileName = fetchLegacyFileName();

        File cookieFile = new File(fileName);
        Assert.assertEquals(exists, cookieFile.exists());
    }

    private void assertCookieFileExists(CookiesFetcher fetcher, boolean exists) {
        String fileName = fetchAbsoluteFilePath(fetcher);

        File cookieFile = new File(fileName);
        Assert.assertEquals(exists, cookieFile.exists());
    }

    @Test
    public void testRestoreCookies_NoOtrProfile_NoExistingFile() throws Exception {
        setupProfileProvider(mProfile1, null);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        assertCookieFileExists(fetcher, false);
        assertLegacyCookieFileExists(false);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

        assertCookieFileExists(fetcher, false);
        assertLegacyCookieFileExists(false);
    }

    @Test
    public void testRestoreCookies_NoOtrProfile_InitialProfile_LegacyExistingFile()
            throws Exception {
        setupProfileProvider(mProfile1, null);
        Mockito.when(mProfile1.isInitialProfile()).thenReturn(true);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        File cookieFile = new File(fetchLegacyFileName());
        try (PrintWriter writer = new PrintWriter(cookieFile)) {
            writer.println("Hey, I\"m a cookie!");
        }

        assertCookieFileExists(fetcher, false);
        assertLegacyCookieFileExists(true);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

        assertCookieFileExists(fetcher, false);
        assertLegacyCookieFileExists(false);
    }

    @Test
    public void testRestoreCookies_NoOtrProfile_InitialProfile_ExistingFile() throws Exception {
        setupProfileProvider(mProfile1, null);
        Mockito.when(mProfile1.isInitialProfile()).thenReturn(true);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        File cookieFile = new File(fetchAbsoluteFilePath(fetcher));
        try (PrintWriter writer = new PrintWriter(cookieFile)) {
            writer.println("Hey, I\"m a cookie!");
        }

        assertCookieFileExists(fetcher, true);
        assertLegacyCookieFileExists(false);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

        assertCookieFileExists(fetcher, false);
        assertLegacyCookieFileExists(false);
    }

    @Test
    public void testRestoreCookies_NoOtrProfile_NotInitialProfile_LegacyExistingFile()
            throws Exception {
        setupProfileProvider(mProfile1, null);
        Mockito.when(mProfile1.isInitialProfile()).thenReturn(false);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        File cookieFile = new File(fetchLegacyFileName());
        try (PrintWriter writer = new PrintWriter(cookieFile)) {
            writer.println("Hey, I\"m a cookie!");
        }

        assertLegacyCookieFileExists(true);
        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();
        assertLegacyCookieFileExists(
                true); // Legacy file should not be deleted for non-initial profiles.
    }

    @Test
    public void testRestoreCookies_HasOtrProfile_NoExistingFile() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);
        assertLegacyCookieFileExists(false);
        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();
        assertLegacyCookieFileExists(false);
    }

    @Test
    public void testPersistRestoreCookies() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        fetcher.persistCookies();
        Mockito.verify(mCookiesFetcherJni)
                .persistCookies(Mockito.eq(mIncognitoProfile1), Mockito.eq(fetcher));

        CanonicalCookie[] cookies = new CanonicalCookie[3];
        cookies[0] = mCookie0;
        cookies[1] = mCookie1;
        cookies[2] = mCookie2;
        fetcher.onCookieFetchFinished(cookies);
        mExecutorRule.runAllBackgroundAndUi();

        assertLegacyCookieFileExists(false);
        assertCookieFileExists(fetcher, true);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

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

        assertLegacyCookieFileExists(false);
        assertCookieFileExists(fetcher, false);
    }

    @Test
    public void testRestoreCookies_InitialProfile_LegacyFile() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        Mockito.when(mProfile1.isInitialProfile()).thenReturn(true);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        CanonicalCookie[] cookies = new CanonicalCookie[3];
        cookies[0] = mCookie0;
        cookies[1] = mCookie1;
        cookies[2] = mCookie2;
        CookiesFetcher.saveFetchedCookiesToDisk(fetchLegacyFileName(), mCipherFactory, cookies);
        mExecutorRule.runAllBackgroundAndUi();

        assertLegacyCookieFileExists(true);
        assertCookieFileExists(fetcher, false);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

        // The legacy file is not attempted to restore due to the cipher key being wiped out during
        // app upgrade.
        Mockito.verify(mCookiesFetcherJni, Mockito.never())
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

        assertLegacyCookieFileExists(false);
        assertCookieFileExists(fetcher, false);
    }

    @Test
    public void testRestoreCookies_NotInitialProfile_LegacyFile() throws Exception {
        setupProfileProvider(mProfile1, mIncognitoProfile1);
        Mockito.when(mProfile1.isInitialProfile()).thenReturn(false);
        CookiesFetcher fetcher = new CookiesFetcher(mProfileProvider, mCipherFactory);

        CanonicalCookie[] cookies = new CanonicalCookie[3];
        cookies[0] = mCookie0;
        cookies[1] = mCookie1;
        cookies[2] = mCookie2;
        CookiesFetcher.saveFetchedCookiesToDisk(fetchLegacyFileName(), mCipherFactory, cookies);
        mExecutorRule.runAllBackgroundAndUi();

        assertLegacyCookieFileExists(true);
        assertCookieFileExists(fetcher, false);

        CallbackHelper restoreCallback = new CallbackHelper();
        fetcher.restoreCookies(restoreCallback::notifyCalled);
        mExecutorRule.runAllBackgroundAndUi();
        restoreCallback.waitForOnly();

        Mockito.verify(mCookiesFetcherJni, Mockito.never())
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

        assertLegacyCookieFileExists(
                true); // Legacy file should not be deleted for non-initial profiles.
        assertCookieFileExists(fetcher, false);
    }
}
