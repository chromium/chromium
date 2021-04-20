// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDialogBridgeJni;
import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogHelper.Source;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.prefs.PrefService;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit test for {@link DownloadLaterDialogHelper}.
 */
@RunWith(LocalRobolectricTestRunner.class)
public class DownloadLaterDialogHelperUnitTest {
    private static final long START_TIME = 100;

    @Mock
    private DownloadLaterDialogCoordinator mDownloadLaterDialog;

    @Mock
    private Context mContext;

    @Mock
    private ModalDialogManager mModalDialogManager;

    @Mock
    private PrefService mPrefService;

    @Mock
    private Callback<OfflineItemSchedule> mMockCallback;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private DownloadDialogBridge.Natives mNativeMock;

    private DownloadLaterDialogHelper mDownloadLaterDialogHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;
        mJniMocker.mock(DownloadDialogBridgeJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.shouldShowDateTimePicker()).thenReturn(true);
        when(mPrefService.getInteger(Pref.DOWNLOAD_LATER_PROMPT_STATUS))
                .thenReturn(DownloadLaterPromptStatus.SHOW_INITIAL);
        doNothing().when(mPrefService).setInteger(anyString(), anyInt());

        mDownloadLaterDialogHelper = new DownloadLaterDialogHelper(
                mContext, mModalDialogManager, mPrefService, mDownloadLaterDialog);
    }

    @Test
    public void testShowDialog() {
        OfflineItemSchedule schedule = new OfflineItemSchedule(true, -1);
        doAnswer(invocation -> {
            mDownloadLaterDialogHelper.onDownloadLaterDialogComplete(
                    DownloadLaterDialogChoice.DOWNLOAD_LATER, START_TIME);
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        mDownloadLaterDialogHelper.showChangeScheduleDialog(
                schedule, Source.DOWNLOAD_HOME, mMockCallback);
        verify(mDownloadLaterDialog, times(1)).showDialog(any(), any(), any(), any());
        ArgumentCaptor<OfflineItemSchedule> captor =
                ArgumentCaptor.forClass(OfflineItemSchedule.class);
        verify(mMockCallback, times(1)).onResult(captor.capture());
        Assert.assertEquals(START_TIME, captor.getValue().startTimeMs);
        Assert.assertFalse(captor.getValue().onlyOnWifi);
    }

    @Test
    public void testShowDialogWithScheduledStartTime() {
        OfflineItemSchedule schedule = new OfflineItemSchedule(false, START_TIME);
        mDownloadLaterDialogHelper.showChangeScheduleDialog(
                schedule, Source.DOWNLOAD_HOME, mMockCallback);
        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);

        verify(mDownloadLaterDialog, times(1)).showDialog(any(), any(), any(), captor.capture());
        PropertyModel model = captor.getValue();
        Assert.assertEquals(
                START_TIME, (long) model.get(DownloadDateTimePickerDialogProperties.INITIAL_TIME));
    }

    @Test
    public void testCancel() {
        OfflineItemSchedule schedule = new OfflineItemSchedule(true, -1);
        doAnswer(invocation -> {
            mDownloadLaterDialogHelper.onDownloadLaterDialogCanceled();
            return null;
        })
                .when(mDownloadLaterDialog)
                .showDialog(any(), any(), any(), any());

        mDownloadLaterDialogHelper.showChangeScheduleDialog(
                schedule, Source.DOWNLOAD_INFOBAR, mMockCallback);
        verify(mDownloadLaterDialog, times(1)).showDialog(any(), any(), any(), any());
        verify(mMockCallback, times(1)).onResult(null);
    }

    @Test
    public void testDestory() {
        mDownloadLaterDialogHelper.destroy();
        verify(mDownloadLaterDialog, times(1)).destroy();
        verify(mMockCallback, times(0)).onResult(any());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.DOWNLOAD_LATER})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.DOWNLOAD_LATER + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:show_date_time_picker/false"})
    public void
    testDownloadHideDownloadLaterDateTimePicker() {
        when(mNativeMock.shouldShowDateTimePicker()).thenReturn(false);
        OfflineItemSchedule schedule = new OfflineItemSchedule(false, START_TIME);
        mDownloadLaterDialogHelper.showChangeScheduleDialog(
                schedule, Source.DOWNLOAD_HOME, mMockCallback);
        ArgumentCaptor<PropertyModel> captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mDownloadLaterDialog, times(1)).showDialog(any(), any(), any(), captor.capture());
        PropertyModel model = captor.getValue();
        Assert.assertFalse(model.get(DownloadLaterDialogProperties.SHOW_DATE_TIME_PICKER_OPTION));
    }
}
