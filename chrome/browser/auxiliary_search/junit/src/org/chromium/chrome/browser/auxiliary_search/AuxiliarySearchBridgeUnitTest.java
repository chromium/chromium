// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AuxiliarySearchBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class AuxiliarySearchBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_ID_1 = 1;
    // Arbitrary non-0 value.
    private static final long NATIVE_BRIDGE = 10L;

    @Mock private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock private Profile mProfile;

    private AuxiliarySearchDataEntry mDataEntry1;
    private AuxiliarySearchBridge mBridge;

    @Before
    public void setUp() {
        AuxiliarySearchBridgeJni.setInstanceForTesting(mMockAuxiliarySearchBridgeJni);
        when(mMockAuxiliarySearchBridgeJni.getForProfile(mProfile)).thenReturn(NATIVE_BRIDGE);

        doReturn(false).when(mProfile).isOffTheRecord();
        mBridge = new AuxiliarySearchBridge(mProfile);
        assertNotNull(mBridge);
    }

    @Test
    @SmallTest
    public void getForProfileTest() {
        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
    }

    @Test
    @SmallTest
    public void tesGetNonSensitiveTabs_NoNative() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mBridge = new AuxiliarySearchBridge(mProfile);

        Tab tab = mock(Tab.class);
        List<Tab> tabList = new ArrayList<>();
        tabList.add(tab);
        Callback callback = mock(Callback.class);
        ThreadUtils.runOnUiThreadBlocking(() -> mBridge.getNonSensitiveTabs(tabList, callback));

        verify(callback).onResult(eq(null));
        verify(mMockAuxiliarySearchBridgeJni, never())
                .getNonSensitiveTabs(eq(NATIVE_BRIDGE), any(), eq(callback));
    }

    @Test
    @SmallTest
    public void testAddDataEntry() {
        List<AuxiliarySearchDataEntry> entryList = createEntryList();

        assertEquals(1, entryList.size());

        AuxiliarySearchDataEntry entry = entryList.get(0);
        assertTrue(mDataEntry1.equals(entry));
        assertEquals(mDataEntry1.hashCode(), entry.hashCode());
    }

    @Test
    @SmallTest
    public void testOnDataReady() {
        List<AuxiliarySearchDataEntry> entryList = new ArrayList<>();
        Callback callback = mock(Callback.class);

        AuxiliarySearchBridge.onDataReady(entryList, callback);
        verify(callback).onResult(eq(entryList));
    }

    @Test
    @SmallTest
    public void testGetNonSensitiveHistoryData() {
        Callback callback = mock(Callback.class);
        ThreadUtils.runOnUiThreadBlocking(() -> mBridge.getNonSensitiveHistoryData(callback));

        verify(mMockAuxiliarySearchBridgeJni)
                .getNonSensitiveHistoryData(eq(NATIVE_BRIDGE), eq(callback));
    }

    @Test
    @SmallTest
    public void tesGetNonSensitiveHistoryData_NoNative() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mBridge = new AuxiliarySearchBridge(mProfile);

        Callback callback = mock(Callback.class);
        ThreadUtils.runOnUiThreadBlocking(() -> mBridge.getNonSensitiveHistoryData(callback));

        verify(callback).onResult(eq(null));
        verify(mMockAuxiliarySearchBridgeJni, never())
                .getNonSensitiveHistoryData(eq(NATIVE_BRIDGE), eq(callback));
    }

    List<AuxiliarySearchDataEntry> createEntryList() {
        List<AuxiliarySearchDataEntry> entryList = new ArrayList<>();

        mDataEntry1 =
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ "Title 1",
                        /* lastActiveTime= */ TimeUtils.uptimeMillis(),
                        /* tabId= */ TAB_ID_1,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0);

        entryList.add(
                new AuxiliarySearchDataEntry(
                        mDataEntry1.type,
                        mDataEntry1.url,
                        mDataEntry1.title,
                        mDataEntry1.lastActiveTime,
                        mDataEntry1.tabId,
                        mDataEntry1.appId,
                        mDataEntry1.visitId,
                        mDataEntry1.score));
        return entryList;
    }
}
