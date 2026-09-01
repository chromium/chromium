// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_CALLBACK;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_LEFT_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_RIGHT_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_STATE_EXTRA;
import static org.chromium.chrome.browser.customtabs.CustomTabsConnection.ON_ACTIVITY_LAYOUT_TOP_EXTRA;

import android.app.PendingIntent;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Network;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.PostMessageServiceConnection;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowProcess;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.SplashImageHolder;
import org.chromium.chrome.browser.customtabs.content.EngagementSignalsHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/** Tests for some parts of {@link CustomTabsConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabsConnectionUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final TemporaryFolder mTemporaryFolder = new TemporaryFolder();
    @Mock private SessionHandler mSessionHandler;

    @Mock private CustomTabsCallback mCallback;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;
    @Mock private EngagementSignalsCallback mEngagementSignalsCallback;
    @Mock private Tab mTab;

    private CustomTabsConnection mConnection;
    private SessionHolder<?> mSessionHolder;
    private CustomTabsSessionToken mSession;
    private PostMessageServiceConnection mPostMessageServiceConnection;
    private PostMessageHandler mPostMessageHandler;

    @Before
    public void setup() {
        CustomTabsConnection.setInstanceForTesting(null);
        mConnection = CustomTabsConnection.getInstance();
        mSession = spy(CustomTabsSessionToken.createMockSessionTokenForTesting());
        mSessionHolder = new SessionHolder<>(mSession);
        when(mSession.getCallback()).thenReturn(mCallback);
        doReturn(mSessionHolder).when(mSessionHandler).getSession();
        SessionDataHolder.getInstance().setActiveHandler(mSessionHandler);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);
        mPostMessageServiceConnection = new PostMessageServiceConnection(mSession) {};
        mPostMessageHandler = new PostMessageHandler(mPostMessageServiceConnection);
    }

    @After
    public void tearDown() {
        SessionDataHolder.getInstance().removeActiveHandler(mSessionHandler);
    }

    @Test
    public void updateVisuals_BottomBarSwipeUpGesture() {
        var bundle = new Bundle();
        var pendingIntent = mock(PendingIntent.class);
        bundle.putParcelable(
                CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION, pendingIntent);
        mConnection.updateVisuals(mSession, bundle);
        verify(mSessionHandler).updateSecondaryToolbarSwipeUpPendingIntent(eq(pendingIntent));
    }

    @Test
    public void onActivityLayout_CallbackIsCalledForNamedMethod() {
        int left = 0;
        int top = 0;
        int right = 100;
        int bottom = 200;

        Bundle bundle = new Bundle();
        bundle.putInt(ON_ACTIVITY_LAYOUT_LEFT_EXTRA, left);
        bundle.putInt(ON_ACTIVITY_LAYOUT_TOP_EXTRA, top);
        bundle.putInt(ON_ACTIVITY_LAYOUT_RIGHT_EXTRA, right);
        bundle.putInt(ON_ACTIVITY_LAYOUT_BOTTOM_EXTRA, bottom);
        bundle.putInt(ON_ACTIVITY_LAYOUT_STATE_EXTRA, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        initSession();
        mConnection.onActivityLayout(
                mSessionHolder, left, top, right, bottom, ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET);

        verify(mCallback).extraCallback(eq(ON_ACTIVITY_LAYOUT_CALLBACK), refEq(bundle));
    }

    @Test
    public void isEngagementSignalsApiAvailable_SupplierSet() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        // Test the supplier takes precedence.
        mConnection.setEngagementSignalsAvailableSupplier(mSession, SupplierUtils.alwaysTrue());
        assertTrue(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
        mConnection.setEngagementSignalsAvailableSupplier(mSession, SupplierUtils.alwaysFalse());
        assertFalse(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
    }

    @Test
    public void isEngagementSignalsApiAvailable_Fallback() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        assertTrue(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        assertFalse(mConnection.isEngagementSignalsApiAvailable(mSession, Bundle.EMPTY));
    }

    @Test
    public void setEngagementSignalsCallback_Available() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(true);
        assertTrue(
                mConnection.setEngagementSignalsCallback(
                        mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertEquals(
                mEngagementSignalsCallback,
                mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSessionHolder));
    }

    @Test
    public void setEngagementSignalsCallback_NotAvailable() {
        initSession();
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted()).thenReturn(false);
        assertFalse(
                mConnection.setEngagementSignalsCallback(
                        mSession, mEngagementSignalsCallback, Bundle.EMPTY));
        assertNull(
                mConnection.mClientManager.getEngagementSignalsCallbackForSession(mSessionHolder));
    }

    @Test
    public void testOnMinimized() {
        initSession();
        mConnection.onMinimized(mSessionHolder);
        verify(mCallback).onMinimized(any(Bundle.class));
    }

    @Test
    public void testOnUnminimized() {
        initSession();
        mConnection.onUnminimized(mSessionHolder);
        verify(mCallback).onUnminimized(any(Bundle.class));
    }

    private void initSession() {
        int uid = 111;
        ShadowProcess.setUid(uid);
        shadowOf(RuntimeEnvironment.getApplication().getApplicationContext().getPackageManager())
                .setPackagesForUid(uid, "test.package.name");
        var handler = new EngagementSignalsHandler(mSession);
        mConnection.mClientManager.newSession(
                mSessionHolder,
                uid,
                Process.myPid(),
                null,
                mPostMessageHandler,
                mPostMessageServiceConnection,
                handler);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SEARCH_IN_CCT)
    public void shouldEnableOmniboxForIntent_featureDisabled() {
        // The logic is currently expected to not even peek in the intent.
        assertFalse(mConnection.shouldEnableOmniboxForIntent(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_IN_CCT)
    public void shouldEnableOmniboxForIntent_featureEnabled() {
        // The logic is currently expected to not even peek in the intent.
        // Omnibox must remain disabled even if the feature flag is on.
        assertFalse(mConnection.shouldEnableOmniboxForIntent(null));
    }

    @Test
    public void notifyOpenInBrowser() {
        initSession();

        mConnection.notifyOpenInBrowser(mSessionHolder, mTab);

        verify(mCallback)
                .extraCallback(
                        eq(CustomTabsConnection.OPEN_IN_BROWSER_CALLBACK), any(Bundle.class));
    }

    @Test
    public void extractTargetNetwork_hasPermission() {
        initSession();
        Network network = mock(Network.class);
        Intent intent = new Intent();
        intent.putExtra(CustomTabsIntent.EXTRA_NETWORK, network);

        var app = RuntimeEnvironment.getApplication();
        shadowOf(app).grantPermissions("android.permission.MAINLINE_NETWORK_STACK");

        assertEquals(network, mConnection.extractTargetNetwork(intent, mSessionHolder));
    }

    @Test
    public void extractTargetNetwork_lacksPermission() {
        initSession();
        Network network = mock(Network.class);
        Intent intent = new Intent();
        intent.putExtra(CustomTabsIntent.EXTRA_NETWORK, network);

        assertNull(mConnection.extractTargetNetwork(intent, mSessionHolder));
    }

    @Test
    public void extractTargetNetwork_noNetwork() {
        initSession();
        Intent intent = new Intent();
        assertNull(mConnection.extractTargetNetwork(intent, mSessionHolder));
    }

    /** Content provider that records whether it was opened. */
    public static class TestSplashImageContentProvider extends ContentProvider {
        static boolean sOpened;
        static File sImageFile;

        @Override
        public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
            sOpened = true;
            return ParcelFileDescriptor.open(sImageFile, ParcelFileDescriptor.MODE_READ_ONLY);
        }

        @Override
        public boolean onCreate() {
            return false;
        }

        @Override
        public Cursor query(
                Uri uri,
                String[] projection,
                String selection,
                String[] selectionArgs,
                String sortOrder) {
            return null;
        }

        @Override
        public String getType(Uri uri) {
            return null;
        }

        @Override
        public Uri insert(Uri uri, ContentValues values) {
            return null;
        }

        @Override
        public int delete(Uri uri, String selection, String[] selectionArgs) {
            return 0;
        }

        @Override
        public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
            return 0;
        }
    }

    private Uri registerSplashImageProvider() throws IOException {
        TestSplashImageContentProvider.sOpened = false;
        TestSplashImageContentProvider.sImageFile = mTemporaryFolder.newFile("splash.png");
        try (FileOutputStream stream =
                new FileOutputStream(TestSplashImageContentProvider.sImageFile)) {
            stream.write("non-empty".getBytes());
        }
        ProviderInfo info = new ProviderInfo();
        info.authority = "org.chromium.test.splash";
        Robolectric.buildContentProvider(TestSplashImageContentProvider.class).create(info);
        return Uri.parse("content://org.chromium.test.splash/splash.png");
    }

    private void setCallerUriPermission(int result) {
        Context context =
                new ContextWrapper(ContextUtils.getApplicationContext()) {
                    @Override
                    public int checkUriPermission(Uri uri, int pid, int uid, int modeFlags) {
                        return result;
                    }
                };
        ContextUtils.initApplicationContextForTests(context);
    }

    @Config(sdk = {BaseRobolectricTestRunner.MIN_SDK, 35})
    @Test
    public void receiveFile_CallerWithoutUriPermission() throws IOException {
        Uri uri = registerSplashImageProvider();
        setCallerUriPermission(PackageManager.PERMISSION_DENIED);

        assertFalse(
                mConnection.receiveFile(
                        mSession,
                        uri,
                        CustomTabsService.FILE_PURPOSE_TRUSTED_WEB_ACTIVITY_SPLASH_IMAGE,
                        Bundle.EMPTY));
        assertFalse(TestSplashImageContentProvider.sOpened);
        assertNull(SplashImageHolder.getInstance().takeImage(mSessionHolder));
    }

    @Config(sdk = {BaseRobolectricTestRunner.MIN_SDK, 35})
    @Test
    public void receiveFile_CallerWithUriPermission() throws IOException {
        initSession();
        Uri uri = registerSplashImageProvider();
        setCallerUriPermission(PackageManager.PERMISSION_GRANTED);

        assertTrue(
                mConnection.receiveFile(
                        mSession,
                        uri,
                        CustomTabsService.FILE_PURPOSE_TRUSTED_WEB_ACTIVITY_SPLASH_IMAGE,
                        Bundle.EMPTY));
        assertTrue(TestSplashImageContentProvider.sOpened);
        assertNotNull(SplashImageHolder.getInstance().takeImage(mSessionHolder));
    }

    @Config(sdk = {BaseRobolectricTestRunner.MIN_SDK, 35})
    @Test
    public void receiveFile_SessionNotRegistered() throws IOException {
        Uri uri = registerSplashImageProvider();
        setCallerUriPermission(PackageManager.PERMISSION_GRANTED);

        assertFalse(
                mConnection.receiveFile(
                        mSession,
                        uri,
                        CustomTabsService.FILE_PURPOSE_TRUSTED_WEB_ACTIVITY_SPLASH_IMAGE,
                        Bundle.EMPTY));
        assertFalse(TestSplashImageContentProvider.sOpened);
        assertNull(SplashImageHolder.getInstance().takeImage(mSessionHolder));
    }

    // TODO(https://crrev.com/c/4118209) Add more tests for Feature enabling/disabling.

    private BaseCustomTabActivity createMockCustomTabActivity(
            boolean hasTargetNetwork, SessionHolder<?> session, boolean finishing) {
        BaseCustomTabActivity activity = mock(BaseCustomTabActivity.class);
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        when(activity.getIntentDataProvider()).thenReturn(provider);
        when(provider.hasTargetNetwork()).thenReturn(hasTargetNetwork);
        doReturn(session).when(provider).getSession();
        when(activity.isFinishing()).thenReturn(finishing);
        return activity;
    }

    @Test
    public void testCleanUpSession_matchingSessionWithTargetNetwork_finishesAndRemovesTask() {
        BaseCustomTabActivity activity =
                createMockCustomTabActivity(
                        /* hasTargetNetwork= */ true, mSessionHolder, /* finishing= */ false);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        try {
            mConnection.cleanUpSession(mSession);
            verify(activity).finishAndRemoveTask();
        } finally {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        }
    }

    @Test
    public void testCleanUpSession_withoutTargetNetwork_doesNotFinish() {
        BaseCustomTabActivity activity =
                createMockCustomTabActivity(
                        /* hasTargetNetwork= */ false, mSessionHolder, /* finishing= */ false);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        try {
            mConnection.cleanUpSession(mSession);
            verify(activity, never()).finishAndRemoveTask();
        } finally {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        }
    }

    @Test
    public void testCleanUpSession_differentSession_doesNotFinish() {
        SessionHolder<?> otherSession =
                new SessionHolder<>(CustomTabsSessionToken.createMockSessionTokenForTesting());
        BaseCustomTabActivity activity =
                createMockCustomTabActivity(
                        /* hasTargetNetwork= */ true, otherSession, /* finishing= */ false);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        try {
            mConnection.cleanUpSession(mSession);
            verify(activity, never()).finishAndRemoveTask();
        } finally {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        }
    }

    @Test
    public void testCleanUpSession_alreadyFinishing_doesNotFinishAgain() {
        BaseCustomTabActivity activity =
                createMockCustomTabActivity(
                        /* hasTargetNetwork= */ true, mSessionHolder, /* finishing= */ true);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        try {
            mConnection.cleanUpSession(mSession);
            verify(activity, never()).finishAndRemoveTask();
        } finally {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        }
    }
}
