// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;
import android.support.customtabs.trusted.TrustedWebActivityServiceConnectionManager;
import android.support.customtabs.trusted.TrustedWebActivityServiceConnectionManager.ExecutionCallback;
import android.support.customtabs.trusted.TrustedWebActivityServiceWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;

/**
 * Unit tests for {@link TrustedWebActivityClient}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityClientTest {

    private static final int SERVICE_SMALL_ICON_ID = 1;

    @Mock
    private TrustedWebActivityServiceConnectionManager mConnection;
    @Mock
    private TrustedWebActivityServiceWrapper mService;
    @Mock
    private NotificationBuilderBase mNotificationBuilder;

    @Mock
    private Bitmap mServiceSmallIconBitmap;

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mConnection.execute(any(), anyString(), any()))
                .thenAnswer((Answer<Boolean>) invocation -> {
                    ExecutionCallback callback = invocation.getArgument(2);
                    callback.onConnected(mService);
                    return true;
                });

        when(mService.getSmallIconId()).thenReturn(SERVICE_SMALL_ICON_ID);
        when(mService.getSmallIconBitmap()).thenReturn(mServiceSmallIconBitmap);

        mClient = new TrustedWebActivityClient(mConnection);
    }

    @Test
    public void usesIconFromService_IfStatusBarIconNotSet() {
        setHasStatusBarBitmap(false);
        postNotification();
        verify(mNotificationBuilder).setStatusBarIconForUntrustedRemoteApp(
                SERVICE_SMALL_ICON_ID, mServiceSmallIconBitmap);
    }


    @Test
    public void doesntUseIconFromService_IfContentBarIconSet() {
        setHasStatusBarBitmap(true);
        postNotification();
        verify(mNotificationBuilder, never())
                .setStatusBarIconForUntrustedRemoteApp(anyInt(), any());
    }

    @Test
    public void usesIconFromService_IfContentSmallIconNotSet() {
        setHasContentBitmap(false);
        postNotification();
        verify(mNotificationBuilder)
                .setContentSmallIconForUntrustedRemoteApp(mServiceSmallIconBitmap);
    }

    @Test
    public void doesntUseIconFromService_IfContentSmallIconSet() {
        setHasContentBitmap(true);
        postNotification();
        verify(mNotificationBuilder, never()).setContentSmallIconForUntrustedRemoteApp(any());
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
        mClient.notifyNotification(Uri.parse(""), "tag", 1, mNotificationBuilder);
    }
}
