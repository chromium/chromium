// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogChoice;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogCoordinator;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.prefs.PrefService;
import org.chromium.net.ConnectionType;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Unit test for {@link DownloadDialogBridge}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadDialogBridgeUnitTest {
    private static final int FAKE_NATIVE_HOLDER = 1;
    private static final long INVALID_START_TIME = -1;
    private static final long START_TIME = 1000;
    private static final long TOTAL_BYTES = 100;
    private static final @ConnectionType int CONNECTION_TYPE = ConnectionType.CONNECTION_3G;
    private static final @DownloadLocationDialogType int LOCATION_DIALOG_TYPE =
            DownloadLocationDialogType.DEFAULT;
    private static final @DownloadLocationDialogType int LOCATION_DIALOG_ERROR_TYPE =
            DownloadLocationDialogType.NAME_CONFLICT;

    private static final String SUGGESTED_PATH = "sdcard/download.txt";
    private static final String NEW_SUGGESTED_PATH = "sdcard/new_download.txt";

    private DownloadDialogBridge mBridge;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    private DownloadDialogBridge.Natives mNativeMock;

    @Mock
    ModalDialogManager mModalDialogManager;

    Activity mActivity;

    @Mock
    DownloadLocationDialogCoordinator mLocationDialog;

    @Mock
    DownloadLaterDialogCoordinator mDownloadLaterDialog;

    @Mock
    private PrefService mPrefService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;
        mJniMocker.mock(DownloadDialogBridgeJni.TEST_HOOKS, mNativeMock);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mBridge =
                new DownloadDialogBridge(FAKE_NATIVE_HOLDER, mDownloadLaterDialog, mLocationDialog);
        mBridge.setPrefServiceForTesting(mPrefService);
        when(mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS))
                .thenReturn(DownloadLaterPromptStatus.SHOW_INITIAL);
    }

    @After
    public void tearDown() {
        mBridge = null;
        mActivity = null;
    }

    private void showDialog() {
        mBridge.showDialog(mActivity, mModalDialogManager, mPrefService, TOTAL_BYTES,
                CONNECTION_TYPE, LOCATION_DIALOG_TYPE, SUGGESTED_PATH, true);
    }

    private void locationDialogWillReturn(String newPath) {
        doAnswer(invocation -> {
            mBridge.onDownloadLocationDialogComplete(newPath);
            return null;
        })
                .when(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), anyInt(), eq(SUGGESTED_PATH));
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testShowDialog_disableDownloadLater() {
        doAnswer(invocation -> {
            mBridge.onDownloadLocationDialogComplete(NEW_SUGGESTED_PATH);
            return null;
        })
                .when(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));

        showDialog();
        verify(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));
        verify(mNativeMock)
                .onComplete(anyLong(), any(), eq(NEW_SUGGESTED_PATH), eq(false),
                        eq(INVALID_START_TIME));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testShowDialog_notShowOnWifi() {
        doAnswer(invocation -> {
            mBridge.onDownloadLocationDialogComplete(NEW_SUGGESTED_PATH);
            return null;
        })
                .when(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));

        mBridge.showDialog(mActivity, mModalDialogManager, mPrefService, TOTAL_BYTES,
                CONNECTION_TYPE, LOCATION_DIALOG_TYPE, SUGGESTED_PATH,
                false /*isOnMeteredNetwork*/);
        verify(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));
        verify(mNativeMock)
                .onComplete(anyLong(), any(), eq(NEW_SUGGESTED_PATH), eq(false),
                        eq(INVALID_START_TIME));
    }

    @Test
    public void testDestroy() {
        mBridge.destroy();
        verify(mLocationDialog).destroy();
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testLocationDialogCanceled_disableDownloadLater() {
        doAnswer(invocation -> {
            mBridge.onDownloadLocationDialogCanceled();
            return null;
        })
                .when(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));

        showDialog();
        verify(mNativeMock).onCanceled(anyLong(), any());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testDownloadLaterComplete_downloadNow() {
        doAnswer(invocation -> {
            mBridge.onDownloadLaterDialogComplete(
                    DownloadLaterDialogChoice.DOWNLOAD_NOW, INVALID_START_TIME);
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        showDialog();
        verify(mDownloadLaterDialog).showDialog(any(), any(), any(), any());
        verify(mNativeMock)
                .onComplete(
                        anyLong(), any(), eq(SUGGESTED_PATH), eq(false), eq(INVALID_START_TIME));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testDownloadLaterComplete_downloadOnWifi() {
        doAnswer(invocation -> {
            mBridge.onDownloadLaterDialogComplete(
                    DownloadLaterDialogChoice.ON_WIFI, INVALID_START_TIME);
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        showDialog();
        verify(mDownloadLaterDialog).showDialog(any(), any(), any(), any());
        verify(mNativeMock)
                .onComplete(anyLong(), any(), eq(SUGGESTED_PATH), eq(true), eq(INVALID_START_TIME));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testDownloadLaterComplete_downloadLater() {
        doAnswer(invocation -> {
            mBridge.onDownloadLaterDialogComplete(
                    DownloadLaterDialogChoice.DOWNLOAD_LATER, START_TIME);
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        showDialog();
        verify(mDownloadLaterDialog).showDialog(any(), any(), any(), any());
        verify(mNativeMock)
                .onComplete(anyLong(), any(), eq(SUGGESTED_PATH), eq(false), eq(START_TIME));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testLocationErrorMakeLocationDialogShow() {
        doAnswer(invocation -> {
            mBridge.onDownloadLaterDialogComplete(
                    DownloadLaterDialogChoice.DOWNLOAD_NOW, INVALID_START_TIME);
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        locationDialogWillReturn(NEW_SUGGESTED_PATH);

        // Location dialog has error message, and it will show.
        mBridge.showDialog(mActivity, mModalDialogManager, mPrefService, TOTAL_BYTES,
                CONNECTION_TYPE, LOCATION_DIALOG_ERROR_TYPE, SUGGESTED_PATH, true);
        verify(mDownloadLaterDialog).showDialog(any(), any(), any(), any());
        verify(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_ERROR_TYPE),
                        eq(SUGGESTED_PATH));
        verify(mNativeMock)
                .onComplete(anyLong(), any(), eq(NEW_SUGGESTED_PATH), eq(false),
                        eq(INVALID_START_TIME));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testClickEditLocationText() {
        locationDialogWillReturn(NEW_SUGGESTED_PATH);

        // Click the "Edit" text to open location dialog.
        mBridge.showDialog(mActivity, mModalDialogManager, mPrefService, TOTAL_BYTES,
                CONNECTION_TYPE, LOCATION_DIALOG_TYPE, SUGGESTED_PATH, true);
        mBridge.onEditLocationClicked();

        // The flow will open download later dialog, then open location dialog, then open download
        // later dialog again.
        verify(mDownloadLaterDialog, times(2)).showDialog(any(), any(), any(), any());
        verify(mLocationDialog)
                .showDialog(any(), any(), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));

        // When finish location dialog, still on download later dialog without completing the flow.
        verify(mNativeMock, times(0)).onComplete(anyLong(), any(), any(), anyBoolean(), anyLong());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    public void testDownloadLaterCancel() {
        doAnswer(invocation -> {
            mBridge.onDownloadLaterDialogCanceled();
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        showDialog();
        verify(mDownloadLaterDialog).showDialog(any(), any(), any(), any());
        verify(mLocationDialog, times(0))
                .showDialog(any(), any(), anyLong(), anyInt(), anyString());
        verify(mNativeMock).onCanceled(anyLong(), any());
    }
}
