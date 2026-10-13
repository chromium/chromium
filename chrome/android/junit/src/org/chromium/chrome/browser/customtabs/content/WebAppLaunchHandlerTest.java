// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.ComponentCaller;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/** Tests for {@link WebAppLaunchHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebAppLaunchHandlerTest {
    // Robolectric does not shadow ContextImpl#checkUriPermission(Uri, int, int, int), and
    // Activity#attach initializes mInitialCaller to a non-null ComponentCaller with uid 0.
    private static class TestActivity extends Activity {
        private final Map<Uri, Integer> mGrantedUriModes = new HashMap<>();
        private int mExpectedPid = -1;
        private int mExpectedUid = -1;
        private @Nullable ComponentCaller mCaller;

        void setUriPermission(Uri uri, int grantedModeFlags) {
            mGrantedUriModes.put(uri, grantedModeFlags);
        }

        void denyUriPermission(Uri uri) {
            mGrantedUriModes.put(uri, /* grantedModeFlags= */ 0);
        }

        @Override
        public int checkUriPermission(Uri uri, int pid, int uid, int modeFlags) {
            if (mExpectedPid != -1 && pid != mExpectedPid) {
                return PackageManager.PERMISSION_DENIED;
            }
            if (mExpectedUid != -1 && uid != mExpectedUid) {
                return PackageManager.PERMISSION_DENIED;
            }
            Integer grantedModes = mGrantedUriModes.get(uri);
            if (grantedModes == null) {
                return PackageManager.PERMISSION_GRANTED;
            }
            return (grantedModes & modeFlags) == modeFlags
                    ? PackageManager.PERMISSION_GRANTED
                    : PackageManager.PERMISSION_DENIED;
        }

        @Override
        public @Nullable ComponentCaller getInitialCaller() {
            return mCaller;
        }

        @Override
        public @Nullable ComponentCaller getCurrentCaller() {
            return mCaller;
        }
    }

    static final int WRONG_CLIENT_MODE = 65;

    public static final String INITIAL_URL = JUnitTestGURLs.INITIAL_URL.getSpec();
    public static final String OTHER_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    public static final String CONTENT_URI = "content://com.a.b.c/a";
    public static final String TEST_PACKAGE_NAME = "com.test";

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock MockWebContents mWebContentsMock;
    @Mock CustomTabActivityNavigationController mNavigationControllerMock;
    @Mock Verifier mVerifierMock;
    @Mock CurrentPageVerifier mCurrentPageVerifierMock;
    @Mock WebAppLaunchHandler.Natives mWebAppLaunchHandlerJniMock;
    @Mock CustomTabsConnection mCustomTabsConnectionMock;
    @Mock CustomTabActivityTabProvider mTabProviderMock;

    private final SessionHolder.CustomTab mSessionHolder =
            SessionHolder.of(CustomTabsSessionToken.createMockSessionTokenForTesting());
    private TestActivity mActivity;
    private FileHandlingData mFileHandlingData;
    private String[] mExpectedFileList = new String[0];
    private boolean[] mExpectedCanWriteList = new boolean[0];

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        WebAppLaunchHandlerJni.setInstanceForTesting(mWebAppLaunchHandlerJniMock);
        CustomTabsConnection.setInstanceForTesting(mCustomTabsConnectionMock);

        when(mVerifierMock.verify(any())).thenReturn(Promise.fulfilled(true));
        when(mCurrentPageVerifierMock.getState())
                .thenReturn(
                        new CurrentPageVerifier.VerificationState(
                                "", "", CurrentPageVerifier.VerificationStatus.SUCCESS));

        when(mCustomTabsConnectionMock.getClientUidForSession(eq(mSessionHolder)))
                .thenReturn(12345);
        when(mCustomTabsConnectionMock.getClientPidForSession(eq(mSessionHolder)))
                .thenReturn(67890);
        when(mWebContentsMock.getLastCommittedUrl()).thenReturn(JUnitTestGURLs.INITIAL_URL);
        when(mTabProviderMock.getInitialTabCreationMode()).thenReturn(TabCreationMode.DEFAULT);
        when(mTabProviderMock.getSpeculatedUrl()).thenReturn(null);

        // Mock PackageManager to resolve our test providers
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        String packageName = context.getPackageName();

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = new ApplicationInfo(context.getApplicationInfo());

        ProviderInfo fileProvider = new ProviderInfo();
        fileProvider.packageName = packageName;
        fileProvider.authority = packageName + ".FileProvider";

        ProviderInfo exactProvider = new ProviderInfo();
        exactProvider.packageName = packageName;
        exactProvider.authority = packageName;

        packageInfo.providers = new ProviderInfo[] {fileProvider, exactProvider};
        shadowOf(pm).installPackage(packageInfo);
    }

    @Test
    public void getClientMode() {
        int clientMode =
                WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.NAVIGATE_EXISTING);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.FOCUS_EXISTING);
        Assert.assertEquals(LaunchHandlerClientMode.FOCUS_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.NAVIGATE_NEW);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_NEW, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(LaunchHandlerClientMode.AUTO);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);

        clientMode = WebAppLaunchHandler.getClientMode(WRONG_CLIENT_MODE);
        Assert.assertEquals(LaunchHandlerClientMode.NAVIGATE_EXISTING, clientMode);
    }

    private WebAppLaunchHandler createWebAppLaunchHandler() {
        return WebAppLaunchHandler.create(
                mVerifierMock,
                mCurrentPageVerifierMock,
                mNavigationControllerMock,
                mWebContentsMock,
                mActivity,
                mTabProviderMock);
    }

    private CustomTabIntentDataProvider createIntentDataProvider(
            @LaunchHandlerClientMode.ClientMode int clientMode, String url) {
        return createIntentDataProvider(clientMode, url, null, false);
    }

    private CustomTabIntentDataProvider createIntentDataProvider(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            @Nullable Intent intent,
            boolean isTrusted) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getLaunchHandlerClientMode()).thenReturn(clientMode);
        when(dataProvider.getUrlToLoad()).thenReturn(url);
        when(dataProvider.getClientPackageName()).thenReturn(TEST_PACKAGE_NAME);
        when(dataProvider.getFileHandlingData()).thenReturn(mFileHandlingData);
        when(dataProvider.getSession()).thenReturn(mSessionHolder);
        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.isTrustedIntent()).thenReturn(isTrusted);
        return dataProvider;
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue) {
        doTestHandleIntent(
                clientMode,
                url,
                expectedLoadUrl,
                expectedNotifyQueue,
                true,
                /* hasSpeculativeNavigation= */ false);
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue,
            boolean expectedVerificationSuccess) {
        doTestHandleIntent(
                clientMode,
                url,
                expectedLoadUrl,
                expectedNotifyQueue,
                expectedVerificationSuccess,
                /* hasSpeculativeNavigation= */ false);
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue,
            boolean expectedVerificationSuccess,
            boolean hasSpeculativeNavigation) {
        doTestHandleIntent(
                clientMode,
                url,
                expectedLoadUrl,
                expectedNotifyQueue,
                expectedVerificationSuccess,
                hasSpeculativeNavigation,
                null);
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue,
            boolean expectedVerificationSuccess,
            boolean hasSpeculativeNavigation,
            @Nullable Intent intent) {
        doTestHandleIntent(
                clientMode,
                url,
                expectedLoadUrl,
                expectedNotifyQueue,
                expectedVerificationSuccess,
                hasSpeculativeNavigation,
                intent,
                /* isTrusted= */ false);
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue,
            boolean expectedVerificationSuccess,
            boolean hasSpeculativeNavigation,
            @Nullable Intent intent,
            boolean isTrusted) {
        clearInvocations(mWebAppLaunchHandlerJniMock, mNavigationControllerMock);
        if (hasSpeculativeNavigation) {
            when(mTabProviderMock.getInitialTabCreationMode()).thenReturn(TabCreationMode.HIDDEN);
            when(mTabProviderMock.getSpeculatedUrl()).thenReturn(url);
        } else {
            when(mTabProviderMock.getInitialTabCreationMode()).thenReturn(TabCreationMode.DEFAULT);
            when(mTabProviderMock.getSpeculatedUrl()).thenReturn(null);
        }
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();

        CustomTabIntentDataProvider dataProvider =
                createIntentDataProvider(clientMode, url, intent, isTrusted);

        if (Objects.equals(url, INITIAL_URL)) {
            launchHandler.handleInitialIntent(dataProvider);
        } else {
            launchHandler.handleNewIntent(dataProvider);
        }

        RobolectricUtil.runAllBackgroundAndUi();

        if (expectedLoadUrl) {
            verify(mNavigationControllerMock, times(1))
                    .navigate(argThat(params -> url.equals(params.getUrl())), any());
        } else {
            verifyNoInteractions(mNavigationControllerMock);
        }

        String expectedScope = ShortcutHelper.getScopeFromUrl(url);
        if (TextUtils.isEmpty(expectedScope)) {
            expectedScope = Uri.parse(url).buildUpon().path("").clearQuery().build().toString();
        }

        if (expectedNotifyQueue) {
            boolean expectedWaitNavigation = Objects.equals(url, INITIAL_URL) || expectedLoadUrl;
            if (expectedWaitNavigation) {
                verify(mWebAppLaunchHandlerJniMock, times(1))
                        .prepareForLaunch(
                                any(),
                                anyLong(),
                                eq(url),
                                eq(TEST_PACKAGE_NAME),
                                eq(mExpectedFileList),
                                eq(mExpectedCanWriteList),
                                eq(expectedScope),
                                eq(hasSpeculativeNavigation));
                verify(mWebAppLaunchHandlerJniMock, times(1))
                        .onLaunchVerified(any(), anyLong(), eq(expectedVerificationSuccess));
                verify(mWebAppLaunchHandlerJniMock, never())
                        .enqueueNonNavigating(any(), any(), any(), any(), any(), any());
            } else {
                verify(mWebAppLaunchHandlerJniMock, times(1))
                        .enqueueNonNavigating(
                                any(),
                                eq(url),
                                eq(TEST_PACKAGE_NAME),
                                eq(mExpectedFileList),
                                eq(mExpectedCanWriteList),
                                eq(expectedScope));
                verify(mWebAppLaunchHandlerJniMock, never())
                        .prepareForLaunch(
                                any(), anyLong(), any(), any(), any(), any(), any(), anyBoolean());
                verify(mWebAppLaunchHandlerJniMock, never())
                        .onLaunchVerified(any(), anyLong(), anyBoolean());
            }
        } else {
            verifyNoInteractions(mWebAppLaunchHandlerJniMock);
        }
    }

    @Test
    public void navigateExisting() {
        doTestHandleIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void focusExisting() {
        doTestHandleIntent(
                LaunchHandlerClientMode.FOCUS_EXISTING,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.FOCUS_EXISTING,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void navigateNew() {
        doTestHandleIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW,
                OTHER_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ false);
    }

    @Test
    public void auto() {
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void wrongClientMode() {
        final int wrongClientMode = 65;
        doTestHandleIntent(
                wrongClientMode,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                wrongClientMode,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void verifierFailed() {
        when(mVerifierMock.verify(any())).thenReturn(Promise.fulfilled(false));
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true,
                /* expectedVerificationSuccess= */ false);
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true,
                /* expectedVerificationSuccess= */ false);
    }

    @Test
    public void filePath() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI)));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void multipleFilePaths() {
        final String secondUri = "content://com.a.b.c/second";
        final String thirdUri = "content://com.a.b.c/third";
        mFileHandlingData =
                new FileHandlingData(
                        Arrays.asList(
                                Uri.parse(CONTENT_URI), Uri.parse(secondUri), Uri.parse(thirdUri)));
        mExpectedFileList = new String[] {CONTENT_URI, secondUri, thirdUri};
        mExpectedCanWriteList = new boolean[] {true, true, true};
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_invalidScheme() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse("file:///foo/bar")));
        mExpectedFileList = new String[0]; // Expect empty because file:// is invalid
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_chromePrivateData() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        Uri privateUri = Uri.parse("content://" + packageName + ".FileProvider/foo");
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI), privateUri));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_chromePrivateDataBypass() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        // Bypass using exactly package name as authority (no trailing dot in prefix check)
        Uri bypassUri1 = Uri.parse("content://" + packageName + "/foo");
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI), bypassUri1));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_chromePrivateDataBypassUserInfo() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        // Bypass using userinfo
        Uri bypassUri1 = Uri.parse("content://user@" + packageName + ".FileProvider/foo");
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI), bypassUri1));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_emptyPath() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse("")));
        mExpectedFileList = new String[0];
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_absolutePath() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse("/absolute/path")));
        mExpectedFileList = new String[0];
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_parentReference() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse("relative/../path")));
        mExpectedFileList = new String[0];
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_relativePath() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse("relative/path")));
        mExpectedFileList = new String[0];
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void filePath_sensitiveRelativePath() {
        mFileHandlingData =
                new FileHandlingData(
                        Arrays.asList(Uri.parse("data/data/com.android.chrome/cookies")));
        mExpectedFileList = new String[0];
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    private void doTestNavigateNewInitialIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode) {
        clearInvocations(mWebAppLaunchHandlerJniMock, mNavigationControllerMock);
        CustomTabIntentDataProvider dataProvider =
                createIntentDataProvider(clientMode, INITIAL_URL);
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        launchHandler.handleInitialIntent(dataProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNull(shadowOf(mActivity).getNextStartedActivity());
        verifyNoInteractions(mNavigationControllerMock);

        String expectedScope = ShortcutHelper.getScopeFromUrl(INITIAL_URL);
        if (TextUtils.isEmpty(expectedScope)) {
            expectedScope =
                    Uri.parse(INITIAL_URL).buildUpon().path("").clearQuery().build().toString();
        }
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .prepareForLaunch(
                        any(),
                        anyLong(),
                        eq(INITIAL_URL),
                        any(),
                        any(),
                        eq(mExpectedCanWriteList),
                        eq(expectedScope),
                        eq(false));
        verify(mWebAppLaunchHandlerJniMock, times(1)).onLaunchVerified(any(), anyLong(), eq(true));
    }

    private void doTestNavigateNewNewIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode, int expectedStartActivityTimes) {
        doTestNavigateNewNewIntent(
                clientMode, expectedStartActivityTimes, /* hasSpeculativeNavigation= */ false);
    }

    private void doTestNavigateNewNewIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            int expectedStartActivityTimes,
            boolean hasSpeculativeNavigation) {
        clearInvocations(mWebAppLaunchHandlerJniMock, mNavigationControllerMock);
        if (hasSpeculativeNavigation) {
            when(mTabProviderMock.getSpeculatedUrl()).thenReturn(OTHER_URL);
        } else {
            when(mTabProviderMock.getSpeculatedUrl()).thenReturn(null);
        }

        CustomTabIntentDataProvider dataProvider = createIntentDataProvider(clientMode, OTHER_URL);
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        launchHandler.handleNewIntent(dataProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        if (expectedStartActivityTimes == 0) {
            Assert.assertNull(startedIntent);
        } else {
            Assert.assertEquals(1, expectedStartActivityTimes);
            Assert.assertNotNull(startedIntent);
            Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
            Assert.assertEquals(Uri.parse(OTHER_URL), startedIntent.getData());
            Assert.assertNotEquals(0, startedIntent.getFlags() & FLAG_ACTIVITY_MULTIPLE_TASK);
            Assert.assertNotEquals(0, startedIntent.getFlags() & FLAG_ACTIVITY_NEW_TASK);
            if (mExpectedFileList.length > 0) {
                Bundle fileBundle =
                        startedIntent.getBundleExtra(
                                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA);
                Assert.assertNotNull(fileBundle);
                Assert.assertEquals(
                        Arrays.stream(mExpectedFileList).map(Uri::parse).toList(),
                        FileHandlingData.fromBundle(fileBundle).uris);
            }
            Assert.assertNull(shadowOf(mActivity).getNextStartedActivity());
        }

        final int expectedOtherTimes = expectedStartActivityTimes == 0 ? 1 : 0;
        verify(mNavigationControllerMock, times(expectedOtherTimes)).navigate(any(), any());
        String expectedScopeOther = ShortcutHelper.getScopeFromUrl(OTHER_URL);
        if (TextUtils.isEmpty(expectedScopeOther)) {
            expectedScopeOther =
                    Uri.parse(OTHER_URL).buildUpon().path("").clearQuery().build().toString();
        }

        if (expectedOtherTimes == 1) {
            verify(mWebAppLaunchHandlerJniMock, times(1))
                    .prepareForLaunch(
                            any(),
                            anyLong(),
                            eq(OTHER_URL),
                            any(),
                            any(),
                            eq(mExpectedCanWriteList),
                            eq(expectedScopeOther),
                            eq(hasSpeculativeNavigation));
            verify(mWebAppLaunchHandlerJniMock, times(1))
                    .onLaunchVerified(any(), anyLong(), eq(true));
        } else {
            verify(mWebAppLaunchHandlerJniMock, never())
                    .prepareForLaunch(
                            any(),
                            anyLong(),
                            eq(OTHER_URL),
                            any(),
                            any(),
                            any(),
                            any(),
                            anyBoolean());
        }
    }

    @Test
    public void navigateNewStartNewTask() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_NEW);
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* expectedStartActivityTimes= */ 1);
    }

    @Test
    public void navigateNewStartNewTask_autoClientMode() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.AUTO);
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.AUTO, /* expectedStartActivityTimes= */ 0);
    }

    @Test
    public void navigateNewStartNewTask_anotherClientMode() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_EXISTING);
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING, /* expectedStartActivityTimes= */ 0);
    }

    @Test
    public void navigateNewSpeculativeNewIntent() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_EXISTING);
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING,
                /* expectedStartActivityTimes= */ 0,
                /* hasSpeculativeNavigation= */ true);
    }

    @Test
    public void navigateNewStartNewTask_fileData() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_NEW);

        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI)));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* expectedStartActivityTimes= */ 1);
    }

    @Test
    public void navigateNewStartNewTask_fileData_chromePrivateData() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_NEW);

        final Uri pwCsv =
                Uri.parse(
                        "content://com.android.chrome.FileProvider/passwords/"
                                + "Chrome%20Passwords.csv");

        mActivity.denyUriPermission(pwCsv);

        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI), pwCsv));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* expectedStartActivityTimes= */ 1);
    }

    @Test
    public void testFileHandling_maliciousAppBlocked() {
        final Uri sensitiveUri = Uri.parse("content://com.victim.app/secret_document.docx");
        mFileHandlingData = new FileHandlingData(Arrays.asList(sensitiveUri));
        mExpectedFileList = new String[0]; // Empty array because unauthorized URI is dropped

        mActivity.denyUriPermission(sensitiveUri);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void testFileHandling_legitimateAppAllowed() {
        // A content:// URI is required here because Android blocks file:// URIs across app
        // boundaries (FileUriExposedException), and Android's URI permission grant system
        // (FLAG_GRANT_READ_URI_PERMISSION) only functions on ContentProvider-backed URIs.
        final Uri authorizedUri =
                Uri.parse("content://com.android.externalstorage.documents/photo.png");
        mFileHandlingData = new FileHandlingData(Arrays.asList(authorizedUri));
        mExpectedFileList = new String[] {authorizedUri.toString()};
        mExpectedCanWriteList = new boolean[] {true};

        mActivity.setUriPermission(
                authorizedUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void testFileHandling_stashedDataUsed() {
        final Uri authorizedUri =
                Uri.parse("content://com.android.externalstorage.documents/photo.png");
        mFileHandlingData = new FileHandlingData(Arrays.asList(authorizedUri));
        mExpectedFileList = new String[] {authorizedUri.toString()};
        mExpectedCanWriteList = new boolean[] {true};

        Intent intent = new Intent();
        FileHandlingData verifiedData = new FileHandlingData(Arrays.asList(authorizedUri));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                verifiedData.toBundle());
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE, new boolean[] {true});

        // Deny URI permission to prove we use the stashed data instead.
        mActivity.denyUriPermission(authorizedUri);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true,
                /* expectedVerificationSuccess= */ true,
                /* hasSpeculativeNavigation= */ false,
                intent,
                /* isTrusted= */ false);
    }

    @Test
    public void currentPageVerifierFailed() {
        when(mCurrentPageVerifierMock.getState())
                .thenReturn(
                        new CurrentPageVerifier.VerificationState(
                                "", "", CurrentPageVerifier.VerificationStatus.FAILURE));
        doTestHandleIntent(
                LaunchHandlerClientMode.FOCUS_EXISTING,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);

        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.FOCUS_EXISTING, /* expectedStartActivityTimes= */ 1);
    }

    @Test
    public void currentPageVerifierFailed_fileData() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_EXISTING);

        when(mCurrentPageVerifierMock.getState())
                .thenReturn(
                        new CurrentPageVerifier.VerificationState(
                                "", "", CurrentPageVerifier.VerificationStatus.FAILURE));

        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI)));
        mExpectedFileList = new String[] {CONTENT_URI};
        mExpectedCanWriteList = new boolean[] {true};
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING, /* expectedStartActivityTimes= */ 1);
    }

    @Test
    public void jniPrepareCalledBeforeVerification() {
        Promise<Boolean> promise = new Promise<>();
        when(mVerifierMock.verify(any())).thenReturn(promise);

        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        CustomTabIntentDataProvider dataProvider =
                createIntentDataProvider(LaunchHandlerClientMode.FOCUS_EXISTING, INITIAL_URL);
        launchHandler.handleInitialIntent(dataProvider);
        RobolectricUtil.runAllBackgroundAndUi();

        String expectedScope = ShortcutHelper.getScopeFromUrl(INITIAL_URL);
        if (TextUtils.isEmpty(expectedScope)) {
            expectedScope =
                    Uri.parse(INITIAL_URL).buildUpon().path("").clearQuery().build().toString();
        }

        verify(mWebAppLaunchHandlerJniMock, times(1))
                .prepareForLaunch(
                        any(),
                        anyLong(),
                        eq(INITIAL_URL),
                        any(),
                        any(),
                        eq(mExpectedCanWriteList),
                        eq(expectedScope),
                        eq(false));
        verify(mWebAppLaunchHandlerJniMock, never())
                .onLaunchVerified(any(), anyLong(), anyBoolean());

        promise.fulfill(true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mWebAppLaunchHandlerJniMock, times(1)).onLaunchVerified(any(), anyLong(), eq(true));
    }

    @Test
    public void testFileHandling_readOnlyPermission() {
        final Uri authorizedUri =
                Uri.parse("content://com.android.externalstorage.documents/photo.png");
        mFileHandlingData = new FileHandlingData(Arrays.asList(authorizedUri));
        mExpectedFileList = new String[] {authorizedUri.toString()};
        mExpectedCanWriteList = new boolean[] {false};

        // Grant read, deny write
        mActivity.setUriPermission(authorizedUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testFileHandling_trampolineFallback() {
        ComponentCaller callerMock = mock(ComponentCaller.class);
        when(callerMock.getUid()).thenReturn(Process.myUid());
        mActivity.mCaller = callerMock;

        final Uri authorizedUri =
                Uri.parse("content://com.android.externalstorage.documents/photo.png");
        mFileHandlingData = new FileHandlingData(Arrays.asList(authorizedUri));
        mExpectedFileList = new String[] {authorizedUri.toString()};
        mExpectedCanWriteList = new boolean[] {true};

        mActivity.mExpectedPid = 67890;
        mActivity.mExpectedUid = 12345;
        mActivity.setUriPermission(
                authorizedUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testFileHandling_trampolineFallback_denied() {
        ComponentCaller callerMock = mock(ComponentCaller.class);
        when(callerMock.getUid()).thenReturn(Process.myUid());
        mActivity.mCaller = callerMock;

        final Uri authorizedUri =
                Uri.parse("content://com.android.externalstorage.documents/photo.png");
        mFileHandlingData = new FileHandlingData(Arrays.asList(authorizedUri));
        mExpectedFileList = new String[0];
        mExpectedCanWriteList = new boolean[0];

        mActivity.mExpectedPid = 67890;
        mActivity.mExpectedUid = 12345;
        mActivity.denyUriPermission(authorizedUri);

        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                INITIAL_URL,
                /* expectedLoadUrl= */ false,
                /* expectedNotifyQueue= */ true);
    }

    @Test
    public void testFilterShareData_StashedDataUsed() {
        Intent intent = new Intent();
        Uri fileUri = Uri.parse("content://com.example/file.jpg");
        ShareData verifiedData = new ShareData("title", "text", Arrays.asList(fileUri));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, verifiedData.toBundle());

        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getIntent()).thenReturn(intent);

        ShareData result =
                WebAppLaunchHandler.filterShareData(dataProvider, mActivity, /* caller= */ null);
        Assert.assertNotNull(result);
        Assert.assertEquals("title", result.title);
        Assert.assertEquals(fileUri, result.uris.get(0));
    }

    @Test
    public void testFilterShareData_RawDataFiltered() {
        Intent intent = new Intent();
        Uri authorizedUri = Uri.parse("content://com.example/authorized.jpg");
        Uri unauthorizedUri = Uri.parse("content://com.example/unauthorized.jpg");
        ShareData rawData =
                new ShareData("title", "text", Arrays.asList(authorizedUri, unauthorizedUri));

        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.getShareData()).thenReturn(rawData);
        when(dataProvider.getSession()).thenReturn(mSessionHolder);

        mActivity.setUriPermission(authorizedUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        mActivity.denyUriPermission(unauthorizedUri);

        ShareData result =
                WebAppLaunchHandler.filterShareData(dataProvider, mActivity, /* caller= */ null);
        Assert.assertNotNull(result);
        Assert.assertEquals("title", result.title);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(authorizedUri, result.uris.get(0));
    }

    @Test
    public void testFilterShareData_InvalidUrisFiltered() {
        Intent intent = new Intent();
        Uri internalUri =
                Uri.parse(
                        "content://"
                                + ContextUtils.getApplicationContext().getPackageName()
                                + ".FileProvider/net_export/sample.txt");
        Uri fileSchemeUri = Uri.parse("file:///sdcard/sample.txt");
        Uri validUri = Uri.parse("content://com.example/valid.jpg");
        ShareData rawData =
                new ShareData("title", "text", Arrays.asList(internalUri, fileSchemeUri, validUri));

        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.getShareData()).thenReturn(rawData);
        when(dataProvider.getSession()).thenReturn(mSessionHolder);

        mActivity.setUriPermission(validUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        mActivity.setUriPermission(internalUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        ShareData result =
                WebAppLaunchHandler.filterShareData(dataProvider, mActivity, /* caller= */ null);
        Assert.assertNotNull(result);
        Assert.assertEquals("title", result.title);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(validUri, result.uris.get(0));
    }

    @Test
    public void testFilterShareData_WebApk_AuthorizedUriKept() throws Exception {
        Intent intent = new Intent();
        Uri authorizedUri = Uri.parse("content://org.chromium.webapk.test/file.jpg");
        ShareData rawData = new ShareData("title", "text", Arrays.asList(authorizedUri));

        BrowserServicesIntentDataProvider dataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        WebApkExtras webApkExtras = createWebApkExtras("org.chromium.webapk.test");

        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.getShareData()).thenReturn(rawData);
        when(dataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        when(dataProvider.getWebApkExtras()).thenReturn(webApkExtras);
        when(dataProvider.getSession()).thenReturn(null);

        shadowOf(mActivity.getPackageManager())
                .setPackagesForUid(10123, "org.chromium.webapk.test");
        mActivity.mExpectedUid = 10123;
        mActivity.setUriPermission(authorizedUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        ShareData result =
                WebAppLaunchHandler.filterShareData(dataProvider, mActivity, /* caller= */ null);
        Assert.assertNotNull(result);
        Assert.assertEquals("title", result.title);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(authorizedUri, result.uris.get(0));
    }

    @Test
    public void testFilterShareData_WebApk_UnauthorizedUriFiltered() throws Exception {
        Intent intent = new Intent();
        Uri unauthorizedUri = Uri.parse("content://com.victim.downloads/sensitive.pdf");
        ShareData rawData = new ShareData("title", "text", Arrays.asList(unauthorizedUri));

        BrowserServicesIntentDataProvider dataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        WebApkExtras webApkExtras = createWebApkExtras("org.chromium.webapk.test");

        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.getShareData()).thenReturn(rawData);
        when(dataProvider.getActivityType()).thenReturn(ActivityType.WEB_APK);
        when(dataProvider.getWebApkExtras()).thenReturn(webApkExtras);
        when(dataProvider.getSession()).thenReturn(null);

        shadowOf(mActivity.getPackageManager())
                .setPackagesForUid(10123, "org.chromium.webapk.test");
        mActivity.mExpectedUid = 10123;
        mActivity.denyUriPermission(unauthorizedUri);

        ShareData result =
                WebAppLaunchHandler.filterShareData(dataProvider, mActivity, /* caller= */ null);
        Assert.assertNotNull(result);
        Assert.assertTrue(result.uris.isEmpty());
    }

    @Test
    public void testFilterFileHandlingData_InvalidUrisFiltered() {
        Intent intent = new Intent();
        Uri internalUri =
                Uri.parse(
                        "content://"
                                + ContextUtils.getApplicationContext().getPackageName()
                                + ".FileProvider/net_export/sample.txt");
        Uri fileSchemeUri = Uri.parse("file:///sdcard/sample.txt");
        Uri validUri = Uri.parse("content://com.example/valid.jpg");
        FileHandlingData rawData =
                new FileHandlingData(Arrays.asList(internalUri, fileSchemeUri, validUri));

        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getIntent()).thenReturn(intent);
        when(dataProvider.getFileHandlingData()).thenReturn(rawData);
        when(dataProvider.getSession()).thenReturn(mSessionHolder);
        when(dataProvider.getUrlToLoad()).thenReturn(INITIAL_URL);
        when(dataProvider.getClientPackageName()).thenReturn(TEST_PACKAGE_NAME);

        mActivity.setUriPermission(validUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        mActivity.setUriPermission(internalUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        launchHandler.handleInitialIntent(dataProvider);

        verify(mWebAppLaunchHandlerJniMock)
                .prepareForLaunch(
                        any(),
                        anyLong(),
                        eq(INITIAL_URL),
                        eq(TEST_PACKAGE_NAME),
                        eq(new String[] {validUri.toString()}),
                        any(),
                        any(),
                        anyBoolean());
    }

    @Test
    public void testCopyShareDataPermissions_InvalidUrisFiltered() {
        Intent sourceIntent = new Intent();
        IBinder sessionBinder = mock(IBinder.class);
        sourceIntent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        Uri internalUri =
                Uri.parse(
                        "content://"
                                + ContextUtils.getApplicationContext().getPackageName()
                                + ".FileProvider/net_export/sample.txt");
        Uri fileSchemeUri = Uri.parse("file:///sdcard/sample.txt");
        Uri validUri = Uri.parse("content://com.example/valid.jpg");
        ShareData rawData =
                new ShareData("title", "text", Arrays.asList(internalUri, fileSchemeUri, validUri));
        sourceIntent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, rawData.toBundle());

        mActivity.setUriPermission(validUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        mActivity.setUriPermission(internalUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        Intent targetIntent = new Intent();
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA,
                new ShareData("sample_title", "sample_text", Arrays.asList(internalUri))
                        .toBundle());
        WebAppLaunchHandler.copyShareDataPermissions(mActivity, sourceIntent, targetIntent);

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(validUri, result.uris.get(0));
    }

    @Test
    public void testCopyShareDataPermissionsForWebApk_InvalidUrisFiltered() throws Exception {
        Intent sourceIntent = new Intent();
        Uri internalUri =
                Uri.parse(
                        "content://"
                                + ContextUtils.getApplicationContext().getPackageName()
                                + ".FileProvider/net_export/sample.txt");
        Uri fileSchemeUri = Uri.parse("file:///sdcard/sample.txt");
        Uri validUri = Uri.parse("content://com.example/valid.jpg");
        sourceIntent.putExtra(
                Intent.EXTRA_STREAM,
                new ArrayList<>(Arrays.asList(internalUri, fileSchemeUri, validUri)));

        shadowOf(mActivity.getPackageManager()).setPackagesForUid(12345, TEST_PACKAGE_NAME);
        mActivity.mExpectedUid = 12345;
        mActivity.setUriPermission(validUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        Intent targetIntent = new Intent();
        WebAppLaunchHandler.copyShareDataPermissionsForWebApk(
                mActivity, sourceIntent, targetIntent, TEST_PACKAGE_NAME);

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(validUri, result.uris.get(0));
    }

    @Test
    public void testCopyShareDataPermissionsForWebApk_UnauthorizedUrisFiltered() throws Exception {
        Intent sourceIntent = new Intent();
        Uri unauthorizedUri = Uri.parse("content://com.victim/secret.pdf");
        sourceIntent.putExtra(Intent.EXTRA_STREAM, new ArrayList<>(Arrays.asList(unauthorizedUri)));

        shadowOf(mActivity.getPackageManager()).setPackagesForUid(12345, TEST_PACKAGE_NAME);
        mActivity.mExpectedUid = 12345;
        mActivity.denyUriPermission(unauthorizedUri);

        Intent targetIntent = new Intent();
        WebAppLaunchHandler.copyShareDataPermissionsForWebApk(
                mActivity, sourceIntent, targetIntent, TEST_PACKAGE_NAME);

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertTrue(result.uris.isEmpty());
    }

    @Test
    public void testCopyShareDataPermissionsForWebApk_PartialFiltering() throws Exception {
        Intent sourceIntent = new Intent();
        Uri authorizedUri = Uri.parse("content://com.example/public.jpg");
        Uri unauthorizedUri = Uri.parse("content://com.victim/secret.pdf");
        sourceIntent.putExtra(
                Intent.EXTRA_STREAM,
                new ArrayList<>(Arrays.asList(authorizedUri, unauthorizedUri)));

        shadowOf(mActivity.getPackageManager()).setPackagesForUid(12345, TEST_PACKAGE_NAME);
        mActivity.mExpectedUid = 12345;
        mActivity.setUriPermission(authorizedUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        mActivity.denyUriPermission(unauthorizedUri);

        Intent targetIntent = new Intent();
        WebAppLaunchHandler.copyShareDataPermissionsForWebApk(
                mActivity, sourceIntent, targetIntent, TEST_PACKAGE_NAME);

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(authorizedUri, result.uris.get(0));
    }

    @Test
    public void testCopyShareDataPermissionsForWebApk_TextOnly() {
        Intent sourceIntent = new Intent();
        sourceIntent.putExtra(Intent.EXTRA_SUBJECT, "Shared Title");
        sourceIntent.putExtra(Intent.EXTRA_TEXT, "Shared Text");

        Intent targetIntent = new Intent();
        WebAppLaunchHandler.copyShareDataPermissionsForWebApk(
                mActivity, sourceIntent, targetIntent, TEST_PACKAGE_NAME);

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertEquals("Shared Title", result.title);
        Assert.assertEquals("Shared Text", result.text);
        Assert.assertTrue(result.uris == null || result.uris.isEmpty());
    }

    @Test
    public void testCopyShareDataPermissionsForWebApk_NameNotFoundExceptionFallback() {
        Intent sourceIntent = new Intent();
        Uri uri = Uri.parse("content://com.example/file.jpg");
        sourceIntent.putExtra(Intent.EXTRA_STREAM, new ArrayList<>(Arrays.asList(uri)));

        Intent targetIntent = new Intent();
        WebAppLaunchHandler.copyShareDataPermissionsForWebApk(
                mActivity, sourceIntent, targetIntent, "unknown.pkg");

        Bundle resultBundle =
                targetIntent.getBundleExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        Assert.assertNotNull(resultBundle);
        ShareData result = ShareData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertTrue(result.uris.isEmpty());
    }

    @Test
    public void testCopyFilePermissions_InvalidUrisFiltered() {
        Intent sourceIntent = new Intent();
        IBinder sessionBinder = mock(IBinder.class);
        sourceIntent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        Uri internalUri =
                Uri.parse(
                        "content://"
                                + ContextUtils.getApplicationContext().getPackageName()
                                + ".FileProvider/net_export/sample.txt");
        Uri fileSchemeUri = Uri.parse("file:///sdcard/sample.txt");
        Uri validUri = Uri.parse("content://com.example/valid.jpg");
        FileHandlingData rawData =
                new FileHandlingData(Arrays.asList(internalUri, fileSchemeUri, validUri));
        sourceIntent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA, rawData.toBundle());

        mActivity.setUriPermission(
                validUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        mActivity.setUriPermission(internalUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);

        Intent targetIntent = new Intent();
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                new FileHandlingData(Arrays.asList(internalUri)).toBundle());
        WebAppLaunchHandler.copyFilePermissions(mActivity, sourceIntent, targetIntent);

        Bundle resultBundle =
                targetIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
        Assert.assertNotNull(resultBundle);
        FileHandlingData result = FileHandlingData.fromBundle(resultBundle);
        Assert.assertNotNull(result);
        Assert.assertEquals(1, result.uris.size());
        Assert.assertEquals(validUri, result.uris.get(0));

        boolean[] canWrite =
                targetIntent.getBooleanArrayExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);
        Assert.assertNotNull(canWrite);
        Assert.assertEquals(1, canWrite.length);
        Assert.assertTrue(canWrite[0]);
    }

    private static WebApkExtras createWebApkExtras(String packageName) {
        return new WebApkExtras(
                packageName,
                new WebappIcon(),
                /* isSplashIconMaskable= */ false,
                /* shellApkVersion= */ 0,
                /* manifestUrl= */ null,
                /* manifestStartUrl= */ null,
                /* manifestId= */ null,
                /* appKey= */ null,
                WebApkDistributor.OTHER,
                new HashMap<>(),
                /* shareTarget= */ null,
                /* isSplashProvidedByWebApk= */ false,
                new ArrayList<>(),
                /* webApkVersionCode= */ 0,
                /* lastUpdateTime= */ 0,
                /* hasCustomName= */ false);
    }
}
