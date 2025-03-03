// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.RequestStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for AuxiliarySearchControllerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchControllerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Profile mProfile;
    @Mock private AuxiliarySearchProvider mAuxiliarySearchProvider;
    @Mock private AuxiliarySearchDonor mAuxiliarySearchDonor;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private AuxiliarySearchHooks mHooks;

    @Captor private ArgumentCaptor<Callback<List<Tab>>> mCallbackCaptor;

    @Captor
    private ArgumentCaptor<Callback<List<AuxiliarySearchDataEntry>>> mEntryReadyCallbackCaptor;

    @Captor private ArgumentCaptor<Callback<Boolean>> mDeleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mBackgroundTaskCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDonationCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor1;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor2;

    private AuxiliarySearchDataEntry mDataEntry1;
    private AuxiliarySearchDataEntry mDataEntry2;
    private AuxiliarySearchControllerImpl mAuxiliarySearchControllerImpl;

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);

        mAuxiliarySearchControllerImpl =
                new AuxiliarySearchControllerImpl(
                        mContext,
                        mProfile,
                        mAuxiliarySearchProvider,
                        mAuxiliarySearchDonor,
                        mFaviconHelper);
    }

    @After
    public void tearDown() {
        mFakeTime.resetTimes();
        mAuxiliarySearchControllerImpl.destroy();
    }

    @Test
    public void testOnResumeWithNative() {
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Search.AuxiliarySearch.DeletionRequestStatus.Tabs",
                                RequestStatus.SUCCESSFUL)
                        .expectIntRecords("Search.AuxiliarySearch.DeleteTime.Tabs", timeDelta)
                        .build();
        mAuxiliarySearchControllerImpl.onResumeWithNative();

        verify(mAuxiliarySearchDonor).deleteAllTabs(mDeleteCallbackCaptor.capture());
        mFakeTime.advanceMillis(timeDelta);

        mDeleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Search.AuxiliarySearch.DeletionRequestStatus.Tabs",
                                RequestStatus.UNSUCCESSFUL)
                        .build();
        mDeleteCallbackCaptor.getValue().onResult(false);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnResumeWithNative_Disabled() {
        AuxiliarySearchUtils.setSharedTabsWithOs(false);
        mAuxiliarySearchControllerImpl =
                new AuxiliarySearchControllerImpl(
                        mContext,
                        mProfile,
                        mAuxiliarySearchProvider,
                        mAuxiliarySearchDonor,
                        mFaviconHelper);
        mAuxiliarySearchControllerImpl.onResumeWithNative();

        verify(mAuxiliarySearchDonor).deleteAllTabs(any(Callback.class));
        assertFalse(mAuxiliarySearchControllerImpl.getHasDeletingTaskForTesting());

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testOnPauseWithNative() {
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.QueryTime.Tabs", timeDelta)
                        .build();
        when(mAuxiliarySearchDonor.canDonate()).thenReturn(true);
        mAuxiliarySearchControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider).getTabsSearchableDataProtoAsync(mCallbackCaptor.capture());
        mFakeTime.advanceMillis(timeDelta);

        mCallbackCaptor.getValue().onResult(new ArrayList<>());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnPauseWithNative_Disabled() {
        when(mAuxiliarySearchDonor.canDonate()).thenReturn(false);
        mAuxiliarySearchControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider, never())
                .getTabsSearchableDataProtoAsync(any(Callback.class));
    }

    @Test
    public void testOnDestroy() {
        assertEquals(1, AuxiliarySearchConfigManager.getInstance().getObserverListSizeForTesting());
        mAuxiliarySearchControllerImpl.register(mActivityLifecycleDispatcher);

        mAuxiliarySearchControllerImpl.destroy();

        verify(mActivityLifecycleDispatcher).unregister(eq(mAuxiliarySearchControllerImpl));

        verify(mFaviconHelper).destroy();

        assertEquals(0, AuxiliarySearchConfigManager.getInstance().getObserverListSizeForTesting());
    }

    @Test
    public void testRegister() {
        mAuxiliarySearchControllerImpl.register(mActivityLifecycleDispatcher);

        verify(mActivityLifecycleDispatcher).register(eq(mAuxiliarySearchControllerImpl));
    }

    @Test
    public void testOnBackgroundTaskStart() {
        AuxiliarySearchEntry entry =
                AuxiliarySearchEntry.newBuilder()
                        .setId(1)
                        .setTitle("Title1")
                        .setUrl("Url1")
                        .setCreationTimestamp(11)
                        .setLastModificationTimestamp(12)
                        .setLastAccessTimestamp(13)
                        .build();
        List<AuxiliarySearchEntry> entries = new ArrayList<>();
        entries.add(entry);

        Map<AuxiliarySearchEntry, Bitmap> map = new HashMap<>();
        Bitmap bitmap = Bitmap.createBitmap(20, 20, Config.RGB_565);
        map.put(entry, bitmap);

        long now = TimeUtils.uptimeMillis();
        int timeDelta = 20;

        // Verifies that Donor won't donate if it can't.
        when(mAuxiliarySearchDonor.canDonate()).thenReturn(false);
        mAuxiliarySearchControllerImpl.onBackgroundTaskStart(
                entries, map, Mockito.mock(Callback.class), now);

        verify(mAuxiliarySearchDonor, never()).donateFavicons(any(), eq(map), any(Callback.class));

        // Verifies that Donor will donate if it can.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.Schedule.DonateTime", timeDelta)
                        .build();
        when(mAuxiliarySearchDonor.canDonate()).thenReturn(true);
        mAuxiliarySearchControllerImpl.onBackgroundTaskStart(
                entries, map, Mockito.mock(Callback.class), now);

        verify(mAuxiliarySearchDonor)
                .donateFavicons(
                        eq(entries), eq(map), mBackgroundTaskCompleteCallbackCaptor.capture());
        mFakeTime.advanceMillis(timeDelta);

        mBackgroundTaskCompleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON)
    public void testOnNonSensitiveTabsAvailable() {
        long now = TimeUtils.uptimeMillis();

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.getTitle()).thenReturn("Title1");
        when(mTab1.getTimestampMillis()).thenReturn(now - 2);

        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.getTitle()).thenReturn("Title2");
        when(mTab2.getTimestampMillis()).thenReturn(now - 1);

        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);

        mAuxiliarySearchControllerImpl.onNonSensitiveTabsAvailable(tabs, now);

        // Verifies the tabs are sorted based on timestamp.
        assertEquals(mTab2, tabs.get(0));
        assertEquals(mTab1, tabs.get(1));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
    })
    public void testOnNonSensitiveHistoryDataAvailable_EmptyList() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.QueryTime.Tabs", timeDelta)
                        .build();

        // Verifies the case when the entry list is empty.
        mFakeTime.advanceMillis(timeDelta);
        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        mAuxiliarySearchControllerImpl.onNonSensitiveHistoryDataAvailable(entries, now);

        histogramWatcher.assertExpected();
        verify(mAuxiliarySearchDonor, never())
                .donateEntries(eq(entries), mDonationCompleteCallbackCaptor.capture());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON)
    public void testOnNonSensitiveDataAvailable() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.getTitle()).thenReturn("Title1");
        when(mTab1.getTimestampMillis()).thenReturn(now - 2);

        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.getTitle()).thenReturn("Title2");
        when(mTab2.getTimestampMillis()).thenReturn(now - 1);

        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);

        testOnNonSensitiveDataAvailableImpl(tabs, now, timeDelta);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
    })
    public void testOnNonSensitiveDataAvailable_AuxiliarySearchDataEntry() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries(now);
        testOnNonSensitiveDataAvailableImpl(entries, now, timeDelta);
    }

    private <T> void testOnNonSensitiveDataAvailableImpl(
            List<T> entries, long startTime, int timeDelta) {
        mAuxiliarySearchControllerImpl.onNonSensitiveDataAvailable(entries, startTime);

        verify(mAuxiliarySearchDonor)
                .donateEntries(eq(entries), mDonationCompleteCallbackCaptor.capture());
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(JUnitTestGURLs.URL_1),
                        anyInt(),
                        mFaviconImageCallbackCaptor1.capture());
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(JUnitTestGURLs.URL_2),
                        anyInt(),
                        mFaviconImageCallbackCaptor2.capture());

        Bitmap bitmap = Bitmap.createBitmap(20, 20, Config.RGB_565);
        mFakeTime.advanceMillis(timeDelta);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AuxiliarySearchMetrics.HISTOGRAM_FAVICON_FIRST_DONATE_COUNT, 1)
                        .expectIntRecord(
                                AuxiliarySearchMetrics.HISTOGRAM_QUERYTIME_FAVICONS, timeDelta)
                        .build();

        mFaviconImageCallbackCaptor1.getValue().onFaviconAvailable(bitmap, null);
        verify(mAuxiliarySearchDonor, never())
                .donateEntries(any(Map.class), mDonationCompleteCallbackCaptor.capture());
        mFaviconImageCallbackCaptor2.getValue().onFaviconAvailable(null, null);
        verify(mAuxiliarySearchDonor)
                .donateEntries(any(Map.class), mDonationCompleteCallbackCaptor.capture());
        histogramWatcher.assertExpected();

        // Verifies the callback is called when the donation completes successfully.
        mFakeTime.advanceMillis(timeDelta);
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonateTime", timeDelta * 2)
                        .expectIntRecords(
                                "Search.AuxiliarySearch.DonationRequestStatus",
                                RequestStatus.SUCCESSFUL)
                        .build();

        mDonationCompleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON)
    public void testOnNonSensitiveTabsAvailable_AfterDestroy() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab1.getTitle()).thenReturn("Title1");
        when(mTab1.getTimestampMillis()).thenReturn(now - 2);

        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mTab2.getTitle()).thenReturn("Title2");
        when(mTab2.getTimestampMillis()).thenReturn(now - 1);

        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);

        when(mAuxiliarySearchDonor.canDonate()).thenReturn(true);
        mAuxiliarySearchControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider).getTabsSearchableDataProtoAsync(mCallbackCaptor.capture());

        mAuxiliarySearchControllerImpl.destroy();
        mFakeTime.advanceMillis(timeDelta);
        mCallbackCaptor.getAllValues().get(0).onResult(tabs);

        verify(mAuxiliarySearchDonor, never()).donateEntries(any(List.class), any(Callback.class));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
    })
    public void testOnNonSensitiveHistoryDataAvailable_AfterDestroy() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        mDataEntry1 =
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ "Title 1",
                        /* lastActiveTime= */ now - 2,
                        /* tabId= */ TAB_ID_1,
                        /* appId= */ null,
                        /* visitId= */ -1);
        mDataEntry2 =
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_2,
                        /* title= */ "Title 2",
                        /* lastActiveTime= */ now - 1,
                        /* tabId= */ TAB_ID_2,
                        /* appId= */ null,
                        /* visitId= */ -1);

        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        entries.add(mDataEntry1);
        entries.add(mDataEntry2);

        when(mAuxiliarySearchDonor.canDonate()).thenReturn(true);
        mAuxiliarySearchControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider)
                .getHistorySearchableDataProtoAsync(mEntryReadyCallbackCaptor.capture());

        mAuxiliarySearchControllerImpl.destroy();
        mFakeTime.advanceMillis(timeDelta);
        mEntryReadyCallbackCaptor.getAllValues().get(0).onResult(entries);

        verify(mAuxiliarySearchDonor, never()).donateEntries(any(List.class), any(Callback.class));
    }

    @Test
    public void testOnConfigChanged() {
        mAuxiliarySearchControllerImpl.onConfigChanged(false);
        verify(mAuxiliarySearchDonor).onConfigChanged(eq(false), any(Callback.class));

        mAuxiliarySearchControllerImpl.onConfigChanged(true);
        verify(mAuxiliarySearchDonor).onConfigChanged(eq(true), any(Callback.class));
    }
}
