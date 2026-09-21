// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link AuxiliarySearchTopSiteProviderBridge} */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchTopSiteProviderBridgeUnitTest {
    // Arbitrary non-0 value.
    private static final long NATIVE_BRIDGE = 10L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private AuxiliarySearchTopSiteProviderBridge.Natives
            mMockAuxiliarySearchTopSiteProviderBridgeJni;

    @Mock private Profile mProfile;

    @Mock
    private AuxiliarySearchTopSiteProviderBridge.Observer
            mAuxiliarySearchTopSiteProviderBridgeObserver;

    private AuxiliarySearchTopSiteProviderBridge mBridge;

    @Before
    public void setUp() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        AuxiliarySearchTopSiteProviderBridgeJni.setInstanceForTesting(
                mMockAuxiliarySearchTopSiteProviderBridgeJni);
        when(mMockAuxiliarySearchTopSiteProviderBridgeJni.init(eq(mProfile)))
                .thenReturn(NATIVE_BRIDGE);

        mBridge = new AuxiliarySearchTopSiteProviderBridge(mProfile);
        assertNotNull(mBridge);
    }

    @Test
    public void testSetObserver() {
        mBridge.setObserver(mAuxiliarySearchTopSiteProviderBridgeObserver);
        assertEquals(
                mAuxiliarySearchTopSiteProviderBridgeObserver, mBridge.getObserverForTesting());
        verify(mMockAuxiliarySearchTopSiteProviderBridgeJni)
                .setObserverAndTrigger(eq(NATIVE_BRIDGE), eq(mBridge));
    }

    @Test
    public void testDestroy() {
        mBridge.setObserver(mAuxiliarySearchTopSiteProviderBridgeObserver);
        assertNotNull(mBridge.getObserverForTesting());

        Mockito.reset(mMockAuxiliarySearchTopSiteProviderBridgeJni);
        mBridge.destroy();
        verify(mMockAuxiliarySearchTopSiteProviderBridgeJni).destroy(eq(NATIVE_BRIDGE));
        assertNull(mBridge.getObserverForTesting());
    }

    @Test
    public void testGetMostVisitedSites() {
        mBridge.getMostVisitedSites();
        verify(mMockAuxiliarySearchTopSiteProviderBridgeJni).getMostVisitedSites(eq(NATIVE_BRIDGE));
    }

    @Test
    public void testOnMostVisitedSitesURLsAvailable() {
        mBridge.setObserver(mAuxiliarySearchTopSiteProviderBridgeObserver);

        List<AuxiliarySearchDataEntry> entryList =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_TopSite(
                        TimeUtils.uptimeMillis());
        mBridge.onMostVisitedSitesURLsAvailable(entryList);
        verify(mAuxiliarySearchTopSiteProviderBridgeObserver)
                .onSiteSuggestionsAvailable(eq(entryList));
    }

    @Test
    public void testOnIconMadeAvailable() {
        mBridge.setObserver(mAuxiliarySearchTopSiteProviderBridgeObserver);

        GURL url = JUnitTestGURLs.URL_1;
        mBridge.onIconMadeAvailable(url);
        verify(mAuxiliarySearchTopSiteProviderBridgeObserver).onIconMadeAvailable(eq(url));
    }
}
