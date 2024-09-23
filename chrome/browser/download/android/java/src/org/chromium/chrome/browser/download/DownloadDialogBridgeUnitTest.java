// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.net.ConnectionType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for {@link DownloadDialogBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadDialogBridgeUnitTest {
    private static final int FAKE_NATIVE_HOLDER = 1;
    private static final long TOTAL_BYTES = 100;
    private static final @ConnectionType int CONNECTION_TYPE = ConnectionType.CONNECTION_3G;
    private static final @DownloadLocationDialogType int LOCATION_DIALOG_TYPE =
            DownloadLocationDialogType.DEFAULT;

    private static final String SUGGESTED_PATH = "sdcard/download.txt";
    private static final String NEW_SUGGESTED_PATH = "sdcard/new_download.txt";

    private DownloadDialogBridge mBridge;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private DownloadDialogBridge.Natives mNativeMock;

    @Mock ModalDialogManager mModalDialogManager;

    Activity mActivity;

    @Mock DownloadLocationDialogCoordinator mLocationDialog;

    @Mock Profile mProfile;

    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;
        mJniMocker.mock(DownloadDialogBridgeJni.TEST_HOOKS, mNativeMock);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mBridge = new DownloadDialogBridge(FAKE_NATIVE_HOLDER, mLocationDialog);
    }

    @After
    public void tearDown() {
        mBridge = null;
        mActivity = null;
    }

    private void showDialog() {
        mBridge.showDialog(
                mActivity,
                mModalDialogManager,
                TOTAL_BYTES,
                CONNECTION_TYPE,
                LOCATION_DIALOG_TYPE,
                SUGGESTED_PATH,
                mProfile);
    }

    private void locationDialogWillReturn(String newPath) {
        doAnswer(
                        invocation -> {
                            mBridge.onDownloadLocationDialogComplete(newPath);
                            return null;
                        })
                .when(mLocationDialog)
                .showDialog(
                        any(), any(), eq(TOTAL_BYTES), anyInt(), eq(SUGGESTED_PATH), eq(mProfile));
    }

    @Test
    public void testShowDialog() {
        doAnswer(
                        invocation -> {
                            mBridge.onDownloadLocationDialogComplete(NEW_SUGGESTED_PATH);
                            return null;
                        })
                .when(mLocationDialog)
                .showDialog(
                        any(),
                        any(),
                        eq(TOTAL_BYTES),
                        eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH),
                        eq(mProfile));

        showDialog();
        verify(mLocationDialog)
                .showDialog(
                        any(),
                        any(),
                        eq(TOTAL_BYTES),
                        eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH),
                        eq(mProfile));
        verify(mNativeMock).onComplete(anyLong(), any(), eq(NEW_SUGGESTED_PATH));
    }

    @Test
    public void testDestroy() {
        mBridge.destroy();
        verify(mLocationDialog).destroy();
    }

    @Test
    public void testLocationDialogCanceled() {
        doAnswer(
                        invocation -> {
                            mBridge.onDownloadLocationDialogCanceled();
                            return null;
                        })
                .when(mLocationDialog)
                .showDialog(
                        any(),
                        any(),
                        eq(TOTAL_BYTES),
                        eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH),
                        eq(mProfile));

        showDialog();
        verify(mNativeMock).onCanceled(anyLong(), any());
    }
}
