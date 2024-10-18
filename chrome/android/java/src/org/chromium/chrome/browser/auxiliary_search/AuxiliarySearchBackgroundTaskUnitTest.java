// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.PersistableBundle;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchBackgroundTask.DonateResult;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Unit tests for AuxiliarySearchBackgroundTask. */
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchBackgroundTaskUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int START_TIME = 1000;
    private static final int CURRENT_TIME = 2000;
    private static final String TITLE_1 = "Title 1";
    private static final String TITLE_2 = "Title 2";
    private static final GURL URL_1 = JUnitTestGURLs.URL_1;
    private static final GURL URL_2 = JUnitTestGURLs.URL_2;

    @Mock private Context mContext;
    @Mock private Profile mProfile;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private AuxiliarySearchController mAuxiliarySearchController;
    @Captor private ArgumentCaptor<FaviconImageCallback> mFaviconImageCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDonateCallbackCaptor;

    private List<AuxiliarySearchEntry> mEntries;
    private TaskParameters mParams;
    private AuxiliarySearchBackgroundTask mTask;

    @Before
    public void setUp() throws Exception {
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        SystemClock.setCurrentTimeMillis(CURRENT_TIME);
        mEntries = new ArrayList<>();
        mEntries.add(
                AuxiliarySearchProvider.createAuxiliarySearchEntry(
                        TAB_ID_1, TITLE_1, URL_1.getSpec(), CURRENT_TIME));
        mEntries.add(
                AuxiliarySearchProvider.createAuxiliarySearchEntry(
                        TAB_ID_2, TITLE_2, URL_2.getSpec(), CURRENT_TIME));

        PersistableBundle bundle = new PersistableBundle();
        bundle.putLong(AuxiliarySearchProvider.TASK_CREATED_TIME, START_TIME);
        mParams =
                TaskParameters.create(TaskIds.AUXILIARY_SEARCH_DONATE_JOB_ID)
                        .addExtras(bundle)
                        .build();
        mTask = new AuxiliarySearchBackgroundTask();
    }

    @Test
    public void testOnStartTaskBeforeNativeLoaded() {
        int result = mTask.onStartTaskBeforeNativeLoaded(mContext, mParams, mTaskFinishedCallback);

        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        verify(mTaskFinishedCallback, never()).taskFinished(anyBoolean());
    }

    @Test
    public void testOnStartTaskWithNative() {
        long expectedStartTimeMs = START_TIME;

        String histogramName = "Search.AuxiliarySearch.Schedule.DelayTime";
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, (int) (CURRENT_TIME - expectedStartTimeMs))
                        .build();

        mTask.onStartTaskWithNative(mContext, mParams, mTaskFinishedCallback);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnStopTaskBeforeNativeLoaded() {
        boolean shouldReschedule = mTask.onStopTaskBeforeNativeLoaded(mContext, mParams);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testOnStopTaskWithNative() {
        boolean shouldReschedule = mTask.onStopTaskWithNative(mContext, mParams);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testOnTabDonateMetadataRead() {
        int timeDelta = 5;
        int faviconSize = 50;
        mTask.onTabDonateMetadataRead(
                mProfile,
                faviconSize,
                CURRENT_TIME + timeDelta,
                mTaskFinishedCallback,
                mFaviconHelper,
                mAuxiliarySearchController,
                mEntries);

        // Verifies request fetching favicons for the two entries.
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(URL_1),
                        eq(faviconSize),
                        mFaviconImageCallbackCaptor.capture());
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(URL_2),
                        eq(faviconSize),
                        mFaviconImageCallbackCaptor.capture());

        Bitmap bitmap1 = Bitmap.createBitmap(50, 50, Bitmap.Config.RGB_565);
        mFaviconImageCallbackCaptor.getAllValues().get(0).onFaviconAvailable(bitmap1, URL_1);
        verify(mAuxiliarySearchController, never())
                .onBackgroundTaskStart(
                        eq(mEntries), any(Map.class), mDonateCallbackCaptor.capture(), anyLong());
        verify(mTaskFinishedCallback, never()).taskFinished(anyBoolean());

        // Verifies that AuxiliarySearchController#onBackgroundTaskStart() is called after two
        // fetching are completed.
        mFaviconImageCallbackCaptor.getAllValues().get(0).onFaviconAvailable(null, URL_1);
        verify(mAuxiliarySearchController)
                .onBackgroundTaskStart(
                        eq(mEntries), any(Map.class), mDonateCallbackCaptor.capture(), anyLong());

        String histogramName = "Search.AuxiliarySearch.Schedule.FaviconDonateResult";
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, DonateResult.SUCCEED)
                        .build();

        // Verifies that mTaskFinishedCallback is notified once the background returns a result.
        mDonateCallbackCaptor.getValue().onResult(true);
        verify(mTaskFinishedCallback).taskFinished(eq(false));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnTabDonateMetadataRead_NoFaviconsAvailable() {
        int timeDelta = 5;
        int faviconSize = 50;
        mTask.onTabDonateMetadataRead(
                mProfile,
                faviconSize,
                CURRENT_TIME + timeDelta,
                mTaskFinishedCallback,
                mFaviconHelper,
                mAuxiliarySearchController,
                mEntries);

        // Verifies request fetching favicons for the two entries.
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(URL_1),
                        eq(faviconSize),
                        mFaviconImageCallbackCaptor.capture());
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(URL_2),
                        eq(faviconSize),
                        mFaviconImageCallbackCaptor.capture());

        // Verifies that AuxiliarySearchController#onBackgroundTaskStart() isn't called since there
        // isn't any favicon available.
        String histogramName = "Search.AuxiliarySearch.Schedule.FaviconDonateResult";
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, DonateResult.NO_DATA)
                        .build();
        mFaviconImageCallbackCaptor.getAllValues().get(0).onFaviconAvailable(null, URL_1);
        mFaviconImageCallbackCaptor.getAllValues().get(0).onFaviconAvailable(null, URL_1);
        verify(mAuxiliarySearchController, never())
                .onBackgroundTaskStart(
                        eq(mEntries), any(Map.class), mDonateCallbackCaptor.capture(), anyLong());

        // Verifies that mTaskFinishedCallback is notified.
        verify(mTaskFinishedCallback).taskFinished(eq(false));
        histogramWatcher.assertExpected();
    }
}
