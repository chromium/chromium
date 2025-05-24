// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Looper;

import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Objects;

/** Tests for {@link WebAppLaunchHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER})
public class WebAppLaunchHandlerTest {
    static final int WRONG_CLIENT_MODE = 65;

    public static final String INITIAL_URL = JUnitTestGURLs.INITIAL_URL.getSpec();
    public static final String OTHER_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    public static final String CONTENT_URI = "content://com.a.b.c/a";
    public static final String TEST_PACKAGE_NAME = "com.test";
    private FileHandlingData mFileHandlingData;
    private String[] mExpectedFileList = new String[0];

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock WebContents mWebContentsMock;
    @Mock CustomTabActivityNavigationController mNavigationControllerMock;
    @Mock Verifier mVerifierMock;
    @Mock CurrentPageVerifier mCurrentPageVerifierMock;
    @Mock Activity mActivityMock;
    @Mock WebAppLaunchHandler.Natives mWebAppLaunchHandlerJniMock;

    @Before
    public void setUp() {
        WebAppLaunchHandlerJni.setInstanceForTesting(mWebAppLaunchHandlerJniMock);

        when(mVerifierMock.verify(any())).thenReturn(Promise.fulfilled(true));
        when(mCurrentPageVerifierMock.getState())
                .thenReturn(
                        new CurrentPageVerifier.VerificationState(
                                "", "", CurrentPageVerifier.VerificationStatus.SUCCESS));
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
                mActivityMock);
    }

    private CustomTabIntentDataProvider createIntentDataProvider(
            @LaunchHandlerClientMode.ClientMode int clientMode, String url) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getLaunchHandlerClientMode()).thenReturn(clientMode);
        when(dataProvider.getUrlToLoad()).thenReturn(url);
        when(dataProvider.getClientPackageName()).thenReturn(TEST_PACKAGE_NAME);
        when(dataProvider.getFileHandlingData()).thenReturn(mFileHandlingData);
        return dataProvider;
    }

    private void doTestHandleIntent(
            @LaunchHandlerClientMode.ClientMode int clientMode,
            String url,
            boolean expectedLoadUrl,
            boolean expectedNotifyQueue) {
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();

        CustomTabIntentDataProvider dataProvider = createIntentDataProvider(clientMode, url);

        if (Objects.equals(url, INITIAL_URL)) {
            launchHandler.handleInitialIntent(dataProvider);
        } else {
            launchHandler.handleNewIntent(dataProvider);
        }

        shadowOf(Looper.getMainLooper()).idle();

        // We never need to start navigation on initial intent in launch handler logic because it
        // has been already stated. We just need to notify launch queue. So expectedLoadUrl is
        // always false for INITIAL_URL
        if (expectedLoadUrl) {
            verify(mNavigationControllerMock, times(1))
                    .navigate(argThat(params -> url.equals(params.getUrl())), any());
        } else {
            verifyNoInteractions(mNavigationControllerMock);
        }

        if (expectedNotifyQueue) {
            boolean expectedWaitNavigation = Objects.equals(url, INITIAL_URL) || expectedLoadUrl;
            verify(mWebAppLaunchHandlerJniMock, times(1))
                    .notifyLaunchQueue(
                            any(),
                            eq(expectedWaitNavigation),
                            eq(url),
                            eq(TEST_PACKAGE_NAME),
                            eq(mExpectedFileList));
            verifyNoInteractions(mActivityMock);
        } else {
            verify(mWebAppLaunchHandlerJniMock, times(0))
                    .notifyLaunchQueue(any(), anyBoolean(), eq(url), any(), any());
        }

        if (clientMode != LaunchHandlerClientMode.NAVIGATE_NEW) verifyNoInteractions(mActivityMock);
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
                /* expectedLoadUrl= */ false,
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
                /* expectedNotifyQueue= */ false);
        doTestHandleIntent(
                LaunchHandlerClientMode.AUTO,
                OTHER_URL,
                /* expectedLoadUrl= */ true,
                /* expectedNotifyQueue= */ false);
    }

    @Test
    public void filePath() {
        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI)));
        mExpectedFileList = new String[] {CONTENT_URI};
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
        final String secondUri = "second_uri.com";
        final String thirdUri = "third_uri.com";
        mFileHandlingData =
                new FileHandlingData(
                        Arrays.asList(
                                Uri.parse(CONTENT_URI), Uri.parse(secondUri), Uri.parse(thirdUri)));
        mExpectedFileList = new String[] {CONTENT_URI, secondUri, thirdUri};
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

    void doTestNavigateNewInitialIntent(Integer clientMode) {
        CustomTabIntentDataProvider dataProvider =
                createIntentDataProvider(clientMode, INITIAL_URL);
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        launchHandler.handleInitialIntent(dataProvider);
        shadowOf(Looper.getMainLooper()).idle();

        verifyNoInteractions(mActivityMock);
        verifyNoInteractions(mNavigationControllerMock);
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), anyBoolean(), any(), any(), any());
    }

    void doTestNavigateNewNewIntent(Integer clientMode, int expectedStartActivityTimes) {

        CustomTabIntentDataProvider dataProvider = createIntentDataProvider(clientMode, OTHER_URL);
        WebAppLaunchHandler launchHandler = createWebAppLaunchHandler();
        launchHandler.handleNewIntent(dataProvider);
        shadowOf(Looper.getMainLooper()).idle();

        if (expectedStartActivityTimes == 0) {
            verifyNoInteractions(mActivityMock);
        } else {
            verify(mActivityMock, times(expectedStartActivityTimes))
                    .startActivity(
                            argThat(
                                    params -> {
                                        if (params == null
                                                || params.getData() == null
                                                || params.getAction() == null) {
                                            return false;
                                        }
                                        return Objects.equals(
                                                        params.getAction(), Intent.ACTION_VIEW)
                                                && OTHER_URL.equals(params.getData().toString())
                                                && (params.getFlags() & FLAG_ACTIVITY_MULTIPLE_TASK)
                                                        != 0
                                                && (params.getFlags() & FLAG_ACTIVITY_NEW_TASK)
                                                        != 0;
                                    }));
        }

        final int expectedOtherTimes = expectedStartActivityTimes == 0 ? 1 : 0;
        verify(mNavigationControllerMock, times(expectedOtherTimes)).navigate(any(), any());
        verify(mWebAppLaunchHandlerJniMock, times(expectedOtherTimes))
                .notifyLaunchQueue(any(), anyBoolean(), eq(OTHER_URL), any(), any());
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), anyBoolean(), eq(INITIAL_URL), any(), any());
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
    public void navigateNewStartNewTask_fileData() {
        doTestNavigateNewInitialIntent(LaunchHandlerClientMode.NAVIGATE_NEW);

        mFileHandlingData = new FileHandlingData(Arrays.asList(Uri.parse(CONTENT_URI)));
        mExpectedFileList = new String[] {CONTENT_URI};
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_NEW, /* expectedStartActivityTimes= */ 1);
        verify(mActivityMock, times(1))
                .grantUriPermission(
                        eq(TEST_PACKAGE_NAME), eq(mFileHandlingData.uris.get(0)), anyInt());
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
        doTestNavigateNewNewIntent(
                LaunchHandlerClientMode.NAVIGATE_EXISTING, /* expectedStartActivityTimes= */ 1);
        verify(mActivityMock, times(1))
                .grantUriPermission(
                        eq(TEST_PACKAGE_NAME), eq(mFileHandlingData.uris.get(0)), anyInt());
    }
}
