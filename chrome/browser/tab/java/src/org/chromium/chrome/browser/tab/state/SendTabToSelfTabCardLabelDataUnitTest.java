// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.ShareActivatedEntryPoint;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;

/** Unit tests for {@link SendTabToSelfTabCardLabelData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfTabCardLabelDataUnitTest {
    private static final String DEVICE_NAME = "Example Phone";
    private static final String GUID = "guid";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private SendTabToSelfTabCardLabelData.Natives mSendTabToSelfTabCardLabelDataNatives;

    private Context mContext;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        PersistedTabDataConfiguration.setUseTestConfig(true);
        mContext = RuntimeEnvironment.application;
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getProfile()).thenReturn(mProfile);
        SendTabToSelfTabCardLabelDataJni.setInstanceForTesting(
                mSendTabToSelfTabCardLabelDataNatives);
    }

    @After
    public void tearDown() {
        mUserDataHost.destroy();
    }

    private SendTabToSelfTabCardLabelData createAndSetLabelData() {
        SendTabToSelfTabCardLabelData data =
                new SendTabToSelfTabCardLabelData(
                        mTab, GUID, DEVICE_NAME, System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, data);
        return data;
    }

    @Test
    public void testUserData() {
        // Ensure no label data exists by default.
        assertNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Attach active label data to the tab.
        createAndSetLabelData();

        // Verify the label data is successfully retrieved and formatted.
        SendTabToSelfTabCardLabelData retrievedData = SendTabToSelfTabCardLabelData.get(mTab);
        assertNotNull(retrievedData);
        assertEquals("From Example Phone", retrievedData.getLabelText(mContext));
    }

    @Test
    public void testUserData_Expired() {
        // Attach expired label data exceeding the 10-day window.
        SendTabToSelfTabCardLabelData data = createAndSetLabelData();
        data.setAdditionTimestampMsForTesting(
                System.currentTimeMillis() - 11L * 24 * 60 * 60 * 1000); // 11 days old

        // Verify accessing expired data automatically removes it from the host.
        assertNull(SendTabToSelfTabCardLabelData.get(mTab));
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
    }

    @Test
    public void testUserData_LabelExpiredButDataPersists() {
        // Attach label data and set it to 6 days old (label expired, but data not expired).
        SendTabToSelfTabCardLabelData data = createAndSetLabelData();
        data.setAdditionTimestampMsForTesting(
                System.currentTimeMillis() - 6L * 24 * 60 * 60 * 1000); // 6 days old

        // Verify data is still retrievable (not expired).
        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Verify shouldShowLabel() is false.
        assertTrue(!data.shouldShowLabel());
    }

    @Test
    public void testUserData_Interacted() {
        // Attach active label data and capture the registered TabObserver.
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        createAndSetLabelData();
        verify(mTab).addObserver(captor.capture());

        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Simulate user interaction triggering onShown.
        captor.getValue().onShown(mTab, TabSelectionType.FROM_USER);

        // Verify the entry is marked as activated.
        verify(mSendTabToSelfTabCardLabelDataNatives)
                .markEntryActivated(mProfile, GUID, ShareActivatedEntryPoint.TAB_STRIP);

        // Verify the user interaction removes the UserData and unregisters the observer.
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
        verify(mTab).removeObserver(captor.getValue());
    }

    @Test
    public void testUserData_ClosedWithoutActivation() {
        // Attach active label data and capture the registered TabObserver.
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        createAndSetLabelData();
        verify(mTab).addObserver(captor.capture());

        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Simulate tab closing.
        when(mTab.isClosing()).thenReturn(true);

        // Simulate tab destruction.
        captor.getValue().onDestroyed(mTab);

        // Verify the entry is marked as closed without activation.
        verify(mSendTabToSelfTabCardLabelDataNatives)
                .markEntryActivated(
                        mProfile,
                        GUID,
                        ShareActivatedEntryPoint.TAB_OR_BROWSER_CLOSED_WITHOUT_ACTIVATION);

        // Verify the destruction removes the UserData and unregisters the observer.
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
        verify(mTab).removeObserver(captor.getValue());
    }

    @Test
    public void testUserData_DestroyedWithoutClosing_Shutdown() {
        // Attach active label data and capture the registered TabObserver.
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        createAndSetLabelData();
        verify(mTab).addObserver(captor.capture());

        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Simulate tab destruction WITHOUT prior closing state (shutdown).
        when(mTab.isClosing()).thenReturn(false);
        captor.getValue().onDestroyed(mTab);

        // Verify that the entry is NOT marked as activated/closed.
        org.mockito.Mockito.verifyNoInteractions(mSendTabToSelfTabCardLabelDataNatives);

        // Verify the data is STILL attached to the tab (not removed/deleted).
        assertNotNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
    }

    @Test
    public void testUserData_Restore() {
        when(mTab.getId()).thenReturn(11);
        when(mTab.isInitialized()).thenReturn(true);

        // Create active label data and save it to mock storage.
        SendTabToSelfTabCardLabelData data = createAndSetLabelData();

        // Simulate a browser restart by removing the in-memory UserData object.
        mUserDataHost.removeUserData(SendTabToSelfTabCardLabelData.class);
        assertNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Verify from() successfully restores the persisted label data from mock storage.
        SendTabToSelfTabCardLabelData.from(
                mTab,
                (res) -> {
                    assertNotNull(res);
                    assertEquals("From Example Phone", res.getLabelText(mContext));
                });
        // Flush pending tasks to ensure the callback is executed.
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testUserData_IdempotentDeleteAndDestroy() {
        when(mTab.getId()).thenReturn(12);
        when(mTab.isInitialized()).thenReturn(true);

        // Call from() twice for an uninitialized/empty tab (which triggers deleteAndDestroy)
        SendTabToSelfTabCardLabelData.from(mTab, (res) -> {});
        SendTabToSelfTabCardLabelData.from(mTab, (res) -> {});

        // Flushing tasks will execute both callbacks in sequence and should not crash.
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testUserData_NegativeCache() {
        when(mTab.getId()).thenReturn(13);
        when(mTab.isInitialized()).thenReturn(true);

        // from() should return the negative cache instance.
        SendTabToSelfTabCardLabelData.from(
                mTab,
                (res) -> {
                    assertNotNull(res);
                    assertTrue(res.isNegativeCache());
                });
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that a SendTabToSelfTabCardLabelData IS attached to the tab as negative cache.
        SendTabToSelfTabCardLabelData attachedData =
                mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class);
        assertNotNull(attachedData);
        assertTrue(attachedData.isNegativeCache());

        // Verify that get() returns the negative cache instance.
        SendTabToSelfTabCardLabelData retrieved = SendTabToSelfTabCardLabelData.get(mTab);
        assertNotNull(retrieved);
        assertTrue(retrieved.isNegativeCache());
    }
}
