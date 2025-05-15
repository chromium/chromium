// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.text.format.DateUtils;

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
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AuxiliarySearchMultiDataControllerImpl} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuxiliarySearchMultiDataControllerImplUnitTest {
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
    @Mock private AuxiliarySearchHooks mHooks;
    @Mock private AuxiliarySearchTopSiteProviderBridge mAuxiliarySearchTopSiteProviderBridge;

    @Captor
    private ArgumentCaptor<Callback<List<AuxiliarySearchDataEntry>>> mEntryReadyCallbackCaptor;

    @Captor private ArgumentCaptor<Callback<Boolean>> mDonationCompleteCallbackCaptor;

    private AuxiliarySearchDataEntry mDataEntry1;
    private AuxiliarySearchDataEntry mDataEntry2;
    private AuxiliarySearchMultiDataControllerImpl mAuxiliarySearchMultiDataControllerImpl;

    @Before
    public void setUp() {
        when(mContext.getResources()).thenReturn(mResources);

        var factory = AuxiliarySearchControllerFactory.getInstance();
        factory.setHooksForTesting(mHooks);
        factory.setSupportMultiDataSourceForTesting(true);
        createController();
    }

    @After
    public void tearDown() {
        mFakeTime.resetTimes();
        mAuxiliarySearchMultiDataControllerImpl.destroy(mActivityLifecycleDispatcher);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON,
    })
    public void testOnNonSensitiveHistoryDataAvailable_EmptyList() {
        Runnable runnableMock = Mockito.mock(Runnable.class);
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.QueryTime.History", timeDelta)
                        .build();

        // Verifies the case when the entry list is empty.
        mFakeTime.advanceMillis(timeDelta);
        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        // Sets mExpectDonating to be false.
        mAuxiliarySearchMultiDataControllerImpl.setExpectDonatingForTesting(false);
        assertFalse(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());
        mAuxiliarySearchMultiDataControllerImpl.onNonSensitiveHistoryDataAvailable(
                entries, now, runnableMock);

        histogramWatcher.assertExpected();
        verify(mAuxiliarySearchDonor, never())
                .donateEntries(eq(entries), any(int[].class), any(Callback.class));
        verify(runnableMock).run();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON})
    public void testOnNonSensitiveHistoryDataAvailable() {
        long now = TimeUtils.uptimeMillis();
        int timeDelta = 50;

        mDataEntry1 =
                new AuxiliarySearchDataEntry(
                        /* type= */ org.chromium.chrome.browser.auxiliary_search
                                .AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ "Title 1",
                        /* lastActiveTime= */ now - 2,
                        /* tabId= */ TAB_ID_1,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0);
        mDataEntry2 =
                new AuxiliarySearchDataEntry(
                        /* type= */ org.chromium.chrome.browser.auxiliary_search
                                .AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_2,
                        /* title= */ "Title 2",
                        /* lastActiveTime= */ now - 1,
                        /* tabId= */ TAB_ID_2,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0);

        List<AuxiliarySearchDataEntry> entries = new ArrayList<>();
        entries.add(mDataEntry1);
        entries.add(mDataEntry2);

        when(mAuxiliarySearchDonor.canDonate()).thenReturn(true);
        mAuxiliarySearchMultiDataControllerImpl.onPauseWithNative();

        verify(mAuxiliarySearchProvider)
                .getHistorySearchableDataProtoAsync(mEntryReadyCallbackCaptor.capture());

        mFakeTime.advanceMillis(timeDelta);
        mEntryReadyCallbackCaptor.getAllValues().get(0).onResult(entries);

        verify(mAuxiliarySearchDonor)
                .donateEntries(
                        eq(entries), any(int[].class), mDonationCompleteCallbackCaptor.capture());
        assertFalse(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());

        mDonationCompleteCallbackCaptor.getValue().onResult(true);
        assertTrue(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());
    }

    @Test
    @SmallTest
    public void testGetMergedList() {
        long now = TimeUtils.uptimeMillis();
        // Verifies the case that both history data list and most visited sites list are null.
        List<AuxiliarySearchDataEntry> mergedList =
                mAuxiliarySearchMultiDataControllerImpl.getMergedList(null);
        assertNull(mergedList);

        // Verifies the case that the most visited sites list is null.
        List<AuxiliarySearchDataEntry> historyEntryList =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries(now);
        mergedList = mAuxiliarySearchMultiDataControllerImpl.getMergedList(historyEntryList);
        assertEquals(historyEntryList, mergedList);

        // Verifies the case that the history data list is null.
        List<AuxiliarySearchDataEntry> mvtList =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_TopSite(
                        JUnitTestGURLs.BLUE_1, JUnitTestGURLs.BLUE_2, now);
        mAuxiliarySearchMultiDataControllerImpl.onSiteSuggestionsAvailable(mvtList);
        mergedList = mAuxiliarySearchMultiDataControllerImpl.getMergedList(null);
        assertEquals(mvtList, mergedList);

        // Verifies the case that both history data list and most visited sites list aren't null.
        mergedList = mAuxiliarySearchMultiDataControllerImpl.getMergedList(historyEntryList);
        assertEquals(4, mergedList.size());
        assertEquals(mvtList.get(0), mergedList.get(0));
        assertEquals(historyEntryList.get(0), mergedList.get(1));
        assertEquals(historyEntryList.get(1), mergedList.get(2));
        assertEquals(mvtList.get(1), mergedList.get(3));

        // Verifies the case that most visited sites list expired.
        long timeDelta = DateUtils.DAY_IN_MILLIS + 1;
        mFakeTime.advanceMillis(timeDelta);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Search.AuxiliarySearch.TopSites.ExpirationDuration")
                        .build();
        mergedList = mAuxiliarySearchMultiDataControllerImpl.getMergedList(historyEntryList);
        assertEquals(historyEntryList, mergedList);
        histogramWatcher.assertExpected();

        // Verifies the case that both history data list and most visited sites list aren't null but
        // with duplications.
        now = TimeUtils.uptimeMillis();
        mvtList = AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_TopSite(now);
        mAuxiliarySearchMultiDataControllerImpl.onSiteSuggestionsAvailable(mvtList);
        mergedList = mAuxiliarySearchMultiDataControllerImpl.getMergedList(historyEntryList);
        assertEquals(2, mergedList.size());
        assertEquals(historyEntryList.get(0), mergedList.get(0));
        assertEquals(historyEntryList.get(1), mergedList.get(1));
    }

    @Test
    public void testOnDeferredStartup() {
        mAuxiliarySearchMultiDataControllerImpl.onDeferredStartup();
        verify(mAuxiliarySearchTopSiteProviderBridge)
                .setObserver(eq(mAuxiliarySearchMultiDataControllerImpl));

        Mockito.reset(mAuxiliarySearchTopSiteProviderBridge);
        mAuxiliarySearchMultiDataControllerImpl.onDeferredStartup();
        verify(mAuxiliarySearchTopSiteProviderBridge, never())
                .setObserver(any(AuxiliarySearchTopSiteProviderBridge.Observer.class));
    }

    @Test
    public void testOnResumeWithNative() {
        mAuxiliarySearchMultiDataControllerImpl.setExpectDonatingForTesting(false);
        assertFalse(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());

        mAuxiliarySearchMultiDataControllerImpl.onResumeWithNative();
        assertTrue(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());
    }

    @Test
    public void testOnDestroy() {
        mAuxiliarySearchMultiDataControllerImpl.onDeferredStartup();
        assertEquals(
                mAuxiliarySearchTopSiteProviderBridge,
                mAuxiliarySearchMultiDataControllerImpl
                        .getAuxiliarySearchTopSiteProviderBridgeForTesting());
        verify(mAuxiliarySearchTopSiteProviderBridge)
                .setObserver(eq(mAuxiliarySearchMultiDataControllerImpl));

        mAuxiliarySearchMultiDataControllerImpl.destroy(mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher).unregister(mAuxiliarySearchMultiDataControllerImpl);
        verify(mAuxiliarySearchTopSiteProviderBridge).destroy();
        assertNull(
                mAuxiliarySearchMultiDataControllerImpl
                        .getAuxiliarySearchTopSiteProviderBridgeForTesting());
    }

    @Test
    public void testOnDestroyCalledByMultipleActivities() {
        ActivityLifecycleDispatcher dispatcher = Mockito.mock(ActivityLifecycleDispatcher.class);
        mAuxiliarySearchMultiDataControllerImpl.register(mActivityLifecycleDispatcher);
        mAuxiliarySearchMultiDataControllerImpl.register(dispatcher);
        mAuxiliarySearchMultiDataControllerImpl.onDeferredStartup();
        verify(mAuxiliarySearchTopSiteProviderBridge)
                .setObserver(eq(mAuxiliarySearchMultiDataControllerImpl));

        // Verifies that the controller doesn't destroy the native bridge when there is reference
        // to it.
        mAuxiliarySearchMultiDataControllerImpl.destroy(dispatcher);
        verify(mAuxiliarySearchTopSiteProviderBridge, never()).destroy();

        // Verifies that the controller will destroy the native bridge when there isn't any
        // reference to it.
        mAuxiliarySearchMultiDataControllerImpl.destroy(mActivityLifecycleDispatcher);
        verify(mAuxiliarySearchTopSiteProviderBridge).destroy();
    }

    private void createController() {
        mAuxiliarySearchMultiDataControllerImpl =
                new AuxiliarySearchMultiDataControllerImpl(
                        mContext,
                        mProfile,
                        mAuxiliarySearchProvider,
                        mAuxiliarySearchDonor,
                        mFaviconHelper,
                        AuxiliarySearchController.AuxiliarySearchHostType.CTA,
                        mAuxiliarySearchTopSiteProviderBridge);
        assertTrue(mAuxiliarySearchMultiDataControllerImpl.getExpectDonatingForTesting());
        mAuxiliarySearchMultiDataControllerImpl.register(mActivityLifecycleDispatcher);
    }
}
