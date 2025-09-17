// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
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
import org.robolectric.annotation.Config;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link AuxiliarySearchTopSiteProviderBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AuxiliarySearchTopSiteProviderBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Arbitrary non-0 value.
    private static final long NATIVE_BRIDGE = 10L;

    @Mock
    private AuxiliarySearchTopSiteProviderBridge.Natives
            mMockAuxiliarySearchTopSiteProviderBridgeJni;

    @Mock private Profile mProfile;

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
        AuxiliarySearchTopSiteProviderBridge.Observer observer =
                mock(AuxiliarySearchTopSiteProviderBridge.Observer.class);

        mBridge.setObserver(observer);
        assertEquals(observer, mBridge.getObserverForTesting());
        verify(mMockAuxiliarySearchTopSiteProviderBridgeJni)
                .setObserverAndTrigger(eq(NATIVE_BRIDGE), eq(mBridge));
    }

    @Test
    public void testDestroy() {
        AuxiliarySearchTopSiteProviderBridge.Observer observer =
                mock(AuxiliarySearchTopSiteProviderBridge.Observer.class);

        mBridge.setObserver(observer);
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
        AuxiliarySearchTopSiteProviderBridge.Observer observer =
                mock(AuxiliarySearchTopSiteProviderBridge.Observer.class);
        mBridge.setObserver(observer);

        List<AuxiliarySearchDataEntry> entryList =
                AuxiliarySearchTestHelper.createAuxiliarySearchDataEntries_TopSite(
                        TimeUtils.uptimeMillis());
        mBridge.onMostVisitedSitesURLsAvailable(entryList);
        verify(observer).onSiteSuggestionsAvailable(eq(entryList));
    }

    @Test
    public void testOnIconMadeAvailable() {
        AuxiliarySearchTopSiteProviderBridge.Observer observer =
                mock(AuxiliarySearchTopSiteProviderBridge.Observer.class);
        mBridge.setObserver(observer);

        GURL url = JUnitTestGURLs.URL_1;
        mBridge.onIconMadeAvailable(url);
        verify(observer).onIconMadeAvailable(eq(url));
    }
}
