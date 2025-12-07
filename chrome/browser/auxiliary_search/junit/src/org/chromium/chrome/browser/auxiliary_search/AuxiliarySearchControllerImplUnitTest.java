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
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchHostType;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.RequestStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
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
    @Captor private ArgumentCaptor<Callback<Boolean>> mDeleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mBackgroundTaskCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDonationCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mFaviconDonationCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor1;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor2;

    @Captor
    private ArgumentCaptor<Callback<List<AuxiliarySearchDataEntry>>> mEntryReadyCallbackCaptor;

    private AuxiliarySearchControllerImpl mAuxiliarySearchControllerImpl;

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);

        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(mHooks);
        createController();
    }

    @After
    public void tearDown() {
        mFakeTime.resetTimes();
        mAuxiliarySearchControllerImpl.destroy(mActivityLifecycleDispatcher);
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

        verify(mAuxiliarySearchDonor).deleteAll(mDeleteCallbackCaptor.capture());
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
                        mFaviconHelper,
                        AuxiliarySearchHostType.CTA);
        mAuxiliarySearchControllerImpl.onResumeWithNative();

        verify(mAuxiliarySearchDonor).deleteAll(any(Callback.class));
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

        mCallbackCaptor.getValue().onResult(Collections.emptyList());
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
    public void testDonateCustomTabs() {
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.QueryTime.CustomTabs", timeDelta)
                        .expectIntRecords("Search.AuxiliarySearch.CustomTabFetchResults.Count", 0)
                        .build();

        long beginTime = 4 * TimeUtils.MILLISECONDS_PER_MINUTE;
        GURL url = JUnitTestGURLs.URL_1;
        mAuxiliarySearchControllerImpl.donateCustomTabs(url, beginTime);
        verify(mAuxiliarySearchProvider)
                .getCustomTabsAsync(
                        eq(url),
                        eq(beginTime - AuxiliarySearchControllerImpl.TIME_RANGE_MS),
                        mEntryReadyCallbackCaptor.capture());

        mFakeTime.advanceMillis(timeDelta);
        mEntryReadyCallbackCaptor.getValue().onResult(Collections.emptyList());
        histogramWatcher.assertExpected();

        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_CustomTabs(
                        TimeUtils.uptimeMillis());
        assertEquals(3, entries.size());
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Search.AuxiliarySearch.CustomTabFetchResults.Count",
                                entries.size())
                        .build();
        mEntryReadyCallbackCaptor.getValue().onResult(entries);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDestroy() {
        int currentSize =
                AuxiliarySearchConfigManager.getInstance().getObserverListSizeForTesting();
        createController();

        assertEquals(
                currentSize + 1,
                AuxiliarySearchConfigManager.getInstance().getObserverListSizeForTesting());
        mAuxiliarySearchControllerImpl.register(mActivityLifecycleDispatcher);

        mAuxiliarySearchControllerImpl.destroy(mActivityLifecycleDispatcher);

        verify(mActivityLifecycleDispatcher).unregister(eq(mAuxiliarySearchControllerImpl));
        verify(mFaviconHelper).destroy();
        assertEquals(
                currentSize,
                AuxiliarySearchConfigManager.getInstance().getObserverListSizeForTesting());
    }

    @Test
    public void testRegister() {
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
    @EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE})
    public void testOnNonSensitiveDataAvailable_AuxiliarySearchDataEntry() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        List<AuxiliarySearchDataEntry> entries =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries(now);
        testOnNonSensitiveDataAvailableImpl(entries, now, timeDelta);
    }

    private <T> void testOnNonSensitiveDataAvailableImpl(
            List<T> entries, long startTime, int timeDelta) {
        mAuxiliarySearchControllerImpl.onNonSensitiveDataAvailable(
                entries, startTime, /* onDonationCompleteRunnable= */ null);

        verify(mAuxiliarySearchDonor)
                .donateEntries(
                        eq(entries), any(int[].class), mDonationCompleteCallbackCaptor.capture());

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

        mFakeTime.advanceMillis(timeDelta);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonateTime", timeDelta)
                        .expectIntRecords(
                                "Search.AuxiliarySearch.DonationRequestStatus",
                                RequestStatus.SUCCESSFUL)
                        .build();

        mDonationCompleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();

        Bitmap bitmap = Bitmap.createBitmap(20, 20, Config.RGB_565);
        mFakeTime.advanceMillis(timeDelta);
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AuxiliarySearchMetrics.HISTOGRAM_FAVICON_FIRST_DONATE_COUNT, 1)
                        .expectIntRecord(
                                AuxiliarySearchMetrics.HISTOGRAM_QUERYTIME_FAVICONS, timeDelta * 2)
                        .build();

        mFaviconImageCallbackCaptor1.getValue().onFaviconAvailable(bitmap, null);
        verify(mAuxiliarySearchDonor, never())
                .donateEntries(any(Map.class), mFaviconDonationCompleteCallbackCaptor.capture());
        mFaviconImageCallbackCaptor2.getValue().onFaviconAvailable(null, null);
        verify(mAuxiliarySearchDonor)
                .donateEntries(any(Map.class), mFaviconDonationCompleteCallbackCaptor.capture());
        histogramWatcher.assertExpected();

        // Verifies the callback is called when the donation completes successfully.
        mFakeTime.advanceMillis(timeDelta);
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonateTime", timeDelta * 3)
                        .expectIntRecords(
                                "Search.AuxiliarySearch.DonationRequestStatus",
                                RequestStatus.SUCCESSFUL)
                        .build();

        mFaviconDonationCompleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();
    }

    @Test
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

        mAuxiliarySearchControllerImpl.destroy(mActivityLifecycleDispatcher);
        mFakeTime.advanceMillis(timeDelta);
        mCallbackCaptor.getAllValues().get(0).onResult(tabs);

        verify(mAuxiliarySearchDonor, never())
                .donateEntries(any(List.class), any(int[].class), any(Callback.class));
    }

    @Test
    public void testOnConfigChanged() {
        mAuxiliarySearchControllerImpl.onConfigChanged(false);
        verify(mAuxiliarySearchDonor).onConfigChanged(eq(false), any(Callback.class));

        mAuxiliarySearchControllerImpl.onConfigChanged(true);
        verify(mAuxiliarySearchDonor).onConfigChanged(eq(true), any(Callback.class));
    }

    private void createController() {
        mAuxiliarySearchControllerImpl =
                new AuxiliarySearchControllerImpl(
                        mContext,
                        mProfile,
                        mAuxiliarySearchProvider,
                        mAuxiliarySearchDonor,
                        mFaviconHelper,
                        AuxiliarySearchHostType.CTA);
        mAuxiliarySearchControllerImpl.register(mActivityLifecycleDispatcher);
    }
}
