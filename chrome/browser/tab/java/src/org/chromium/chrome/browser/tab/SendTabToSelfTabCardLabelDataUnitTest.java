// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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

/** Unit tests for {@link SendTabToSelfTabCardLabelData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfTabCardLabelDataUnitTest {
    private static final String DEVICE_NAME = "Example Phone";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    private Context mContext;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
    }

    @After
    public void tearDown() {
        mUserDataHost.destroy();
    }

    @Test
    public void testUserData() {
        // Ensure no label data exists by default.
        assertNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Attach active label data to the tab.
        SendTabToSelfTabCardLabelData data =
                new SendTabToSelfTabCardLabelData(mTab, DEVICE_NAME, System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, data);

        // Verify the label data is successfully retrieved and formatted.
        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));
        assertEquals("From " + DEVICE_NAME, data.getLabelText(mContext));
    }

    @Test
    public void testUserData_Expired() {
        // Attach expired label data exceeding the 5-day window.
        SendTabToSelfTabCardLabelData data =
                new SendTabToSelfTabCardLabelData(
                        mTab,
                        DEVICE_NAME,
                        // Setting an expired timestamp will cause an assertion failure, so that's
                        // done separately via a setter.
                        System.currentTimeMillis());
        data.setAdditionTimestampMsForTesting(
                System.currentTimeMillis() - 6L * 24 * 60 * 60 * 1000); // 6 days old
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, data);

        // Verify accessing expired data automatically removes it from the host.
        assertNull(SendTabToSelfTabCardLabelData.get(mTab));
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
    }

    @Test
    public void testUserData_Interacted() {
        // Attach active label data and capture the registered TabObserver.
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        SendTabToSelfTabCardLabelData data =
                new SendTabToSelfTabCardLabelData(mTab, DEVICE_NAME, System.currentTimeMillis());
        mUserDataHost.setUserData(SendTabToSelfTabCardLabelData.class, data);
        verify(mTab).addObserver(captor.capture());

        assertNotNull(SendTabToSelfTabCardLabelData.get(mTab));

        // Simulate user interaction triggering onShown.
        captor.getValue().onShown(mTab, TabSelectionType.FROM_USER);

        // Verify the user interaction removes the UserData and unregisters the observer.
        assertNull(mUserDataHost.getUserData(SendTabToSelfTabCardLabelData.class));
        verify(mTab).removeObserver(data);
    }
}
