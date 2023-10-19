// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;

import androidx.browser.trusted.Token;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.embedder_support.util.Origin;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link TrustedWebActivityClient}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityClientTest {
    private static final int SERVICE_SMALL_ICON_ID = 1;
    private static final String CLIENT_PACKAGE_NAME = "com.example.app";

    @Mock private TrustedWebActivityClientWrappers.ConnectionPool mConnectionPool;
    @Mock private TrustedWebActivityClientWrappers.Connection mService;
    @Mock private NotificationBuilderBase mNotificationBuilder;
    @Mock private TrustedWebActivityUmaRecorder mRecorder;
    @Mock private NotificationUmaTracker mNotificationUmaTracker;

    @Mock private Bitmap mServiceSmallIconBitmap;
    @Mock private NotificationWrapper mNotificationWrapper;
    @Mock private InstalledWebappPermissionManager mPermissionManager;

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws RemoteException {
        MockitoAnnotations.initMocks(this);

        doAnswer(
                        invocation -> {
                            Origin origin = invocation.getArgument(1);
                            TrustedWebActivityClient.ExecutionCallback callback =
                                    invocation.getArgument(3);

                            callback.onConnected(origin, mService);

                            return null;
                        })
                .when(mConnectionPool)
                .connectAndExecute(any(), any(), any(), any());

        when(mService.getSmallIconId()).thenReturn(SERVICE_SMALL_ICON_ID);
        when(mService.getSmallIconBitmap()).thenReturn(mServiceSmallIconBitmap);
        when(mService.getComponentName()).thenReturn(new ComponentName(CLIENT_PACKAGE_NAME, ""));
        when(mService.areNotificationsEnabled(any())).thenReturn(true);
        when(mNotificationBuilder.build(any())).thenReturn(mNotificationWrapper);

        Set<Token> delegateApps = new HashSet<>();
        delegateApps.add(createDummyToken());
        when(mPermissionManager.getAllDelegateApps(any())).thenReturn(delegateApps);

        mClient = new TrustedWebActivityClient(mConnectionPool, mPermissionManager, mRecorder);
    }

    @Test
    public void usesIconFromService_IfStatusBarIconNotSet() {
        setHasStatusBarBitmap(false);
        postNotification();
        verify(mNotificationBuilder)
                .setStatusBarIconForRemoteApp(SERVICE_SMALL_ICON_ID, mServiceSmallIconBitmap);
    }

    @Test
    public void doesntUseIconFromService_IfContentBarIconSet() {
        setHasStatusBarBitmap(true);
        postNotification();
        verify(mNotificationBuilder, never()).setStatusBarIconForRemoteApp(anyInt(), any());
    }

    @Test
    public void usesIconFromService_IfContentSmallIconNotSet()
            throws ExecutionException, InterruptedException {
        setHasContentBitmap(false);
        postNotification();

        verify(mNotificationBuilder).setContentSmallIconForRemoteApp(mServiceSmallIconBitmap);
    }

    @Test
    public void doesntUseIconFromService_IfContentSmallIconSet() {
        setHasContentBitmap(true);
        postNotification();
        verify(mNotificationBuilder, never()).setContentSmallIconForRemoteApp(any());
    }

    @Test
    public void doesntFetchIconIdFromService_IfBothIconsAreSet() throws RemoteException {
        setHasContentBitmap(true);
        setHasStatusBarBitmap(true);
        postNotification();
        verify(mService, never()).getSmallIconId();
    }

    @Test
    public void doesntFetchIconBitmapFromService_IfIconsIdIs() throws RemoteException {
        setHasContentBitmap(false);
        when(mService.getSmallIconId()).thenReturn(-1);
        postNotification();
        verify(mService, never()).getSmallIconBitmap();
    }

    private void setHasStatusBarBitmap(boolean hasBitmap) {
        when(mNotificationBuilder.hasStatusBarIconBitmap()).thenReturn(hasBitmap);
    }

    private void setHasContentBitmap(boolean hasBitmap) {
        when(mNotificationBuilder.hasSmallIconForContent()).thenReturn(hasBitmap);
    }

    private void postNotification() {
        Uri uri = Uri.parse("https://www.example.com");
        mClient.notifyNotification(uri, "tag", 1, mNotificationBuilder, mNotificationUmaTracker);
    }

    @Test
    public void createLaunchIntentForTwaNonHttpScheme() {
        assertNull(
                TrustedWebActivityClient.createLaunchIntentForTwa(
                        RuntimeEnvironment.application,
                        "mailto:miranda@example.com",
                        new ArrayList<ResolveInfo>()));
    }

    private static Token createDummyToken() {
        // This code requires understanding how Token's parse (see TokenContents.java inside
        // androidx.browser) and is pretty ugly. The alternative is to set up the Robolectric
        // PackageManager to provide the right data, which probably is a more robust approach.
        // However, ideally androidx.browser will add a way to create a mock Token for testing and
        // we can use that instead.
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream writer = new DataOutputStream(baos);

        String packageName = "token.package.name";
        int numFingerprints = 1;
        byte[] fingerprint = "1234".getBytes();

        try {
            writer.writeUTF(packageName);
            writer.writeInt(numFingerprints);
            writer.writeInt(fingerprint.length);
            writer.write(fingerprint);
            writer.flush();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return Token.deserialize(baos.toByteArray());
    }
}
