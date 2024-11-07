// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
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
import org.chromium.base.test.util.JniMocker;
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
    @Rule public JniMocker mJniMocker = new JniMocker();
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

    @Captor private ArgumentCaptor<Callback<List<Tab>>> mCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDeleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mBackgroundTaskCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDonationCompleteCallbackCaptor;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor1;
    @Captor private ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconImageCallbackCaptor2;

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
    public void testOnPauseWithNative() {
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.QueryTime.Tabs", timeDelta)
                        .build();
        mAuxiliarySearchControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider).getTabsSearchableDataProtoAsync(mCallbackCaptor.capture());
        mFakeTime.advanceMillis(timeDelta);

        mCallbackCaptor.getValue().onResult(new ArrayList<>());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDestroy() {
        mAuxiliarySearchControllerImpl.register(mActivityLifecycleDispatcher);

        mAuxiliarySearchControllerImpl.destroy();

        verify(mActivityLifecycleDispatcher).unregister(eq(mAuxiliarySearchControllerImpl));
        verify(mAuxiliarySearchDonor).destroy();

        verify(mFaviconHelper).destroy();
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

        Map<Integer, Bitmap> map = new HashMap<>();
        Bitmap bitmap = Bitmap.createBitmap(20, 20, Config.RGB_565);
        map.put(entry.getId(), bitmap);

        long now = TimeUtils.uptimeMillis();
        int timeDelta = 20;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.Schedule.DonateTime", timeDelta)
                        .build();
        mAuxiliarySearchControllerImpl.onBackgroundTaskStart(
                entries, map, Mockito.mock(Callback.class), now);

        verify(mAuxiliarySearchDonor)
                .donateTabs(eq(entries), eq(map), mBackgroundTaskCompleteCallbackCaptor.capture());
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

        verify(mAuxiliarySearchDonor)
                .donateTabs(eq(tabs), mDonationCompleteCallbackCaptor.capture());
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
        int timeDelta = 50;
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
                .donateTabs(any(Map.class), mDonationCompleteCallbackCaptor.capture());
        mFaviconImageCallbackCaptor2.getValue().onFaviconAvailable(null, null);
        verify(mAuxiliarySearchDonor)
                .donateTabs(any(Map.class), mDonationCompleteCallbackCaptor.capture());
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonateTime", timeDelta)
                        .expectIntRecords(
                                "Search.AuxiliarySearch.DonationRequestStatus",
                                RequestStatus.SUCCESSFUL)
                        .build();

        mDonationCompleteCallbackCaptor.getValue().onResult(true);
        histogramWatcher.assertExpected();
    }
}
