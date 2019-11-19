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

import android.content.ComponentName;
import android.graphics.Bitmap;
import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;

import androidx.browser.trusted.TrustedWebActivityServiceConnectionManager;
import androidx.browser.trusted.TrustedWebActivityServiceConnectionManager.ExecutionCallback;
import androidx.browser.trusted.TrustedWebActivityServiceWrapper;

/**
 * Unit tests for {@link TrustedWebActivityClient}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityClientTest {

    private static final int SERVICE_SMALL_ICON_ID = 1;
    private static final String CLIENT_PACKAGE_NAME = "com.example.app";

    @Mock
    private TrustedWebActivityServiceConnectionManager mConnection;
    @Mock
    private TrustedWebActivityServiceWrapper mService;
    @Mock
    private NotificationBuilderBase mNotificationBuilder;
    @Mock
    private TrustedWebActivityUmaRecorder mRecorder;
    @Mock
    private NotificationUmaTracker mNotificationUmaTracker;

    @Mock
    private Bitmap mServiceSmallIconBitmap;

    @Mock
    private ChromeNotification mChromeNotification;

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mConnection.execute(any(), anyString(), any()))
                .thenAnswer((Answer<Boolean>) invocation -> {
                    ExecutionCallback callback = invocation.getArgument(2);
                    callback.onConnected(mService);
                    return true;
                });

        when(mService.getSmallIconId()).thenReturn(SERVICE_SMALL_ICON_ID);
        when(mService.getSmallIconBitmap()).thenReturn(mServiceSmallIconBitmap);
        when(mService.getComponentName()).thenReturn(new ComponentName(CLIENT_PACKAGE_NAME, ""));
        when(mService.areNotificationsEnabled(any())).thenReturn(true);
        when(mNotificationBuilder.build(any())).thenReturn(mChromeNotification);

        mClient = new TrustedWebActivityClient(mConnection, mRecorder);
    }

    @Test
    public void usesIconFromService_IfStatusBarIconNotSet() {
        setHasStatusBarBitmap(false);
        postNotification();
        verify(mNotificationBuilder)
                .setStatusBarIconForRemoteApp(
                        SERVICE_SMALL_ICON_ID, mServiceSmallIconBitmap, CLIENT_PACKAGE_NAME);
    }


    @Test
    public void doesntUseIconFromService_IfContentBarIconSet() {
        setHasStatusBarBitmap(true);
        postNotification();
        verify(mNotificationBuilder, never())
                .setStatusBarIconForRemoteApp(anyInt(), any(), anyString());
    }

    @Test
    public void usesIconFromService_IfContentSmallIconNotSet() {
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
    public void doesntFetchIconIdFromService_IfBothIconsAreSet() {
        setHasContentBitmap(true);
        setHasStatusBarBitmap(true);
        postNotification();
        verify(mService, never()).getSmallIconId();
    }

    @Test
    public void doesntFetchIconBitmapFromService_IfIconsIdIs() {
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
        mClient.notifyNotification(uri, "tag", 1, mNotificationBuilder,
                mNotificationUmaTracker);
    }
}
