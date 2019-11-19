// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generate_java_test.py

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for {@link PrefServiceBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrefServiceBridgeTest {
    private static final int PREF = 42;

    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private PrefServiceBridge.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(PrefServiceBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testGetBoolean() {
        boolean expected = false;

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        doReturn(expected).when(mNativeMock).getBoolean(PREF);

        assertEquals(expected, prefServiceBridge.getBoolean(PREF));
    }

    @Test
    public void testSetBoolean() {
        boolean value = true;

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        prefServiceBridge.setBoolean(PREF, value);

        verify(mNativeMock).setBoolean(eq(PREF), eq(value));
    }

    @Test
    public void testGetInteger() {
        int expected = 26;

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        doReturn(expected).when(mNativeMock).getInteger(PREF);

        assertEquals(expected, prefServiceBridge.getInteger(PREF));
    }

    @Test
    public void testSetInteger() {
        int value = 62;

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        prefServiceBridge.setInteger(PREF, value);

        verify(mNativeMock).setInteger(eq(PREF), eq(value));
    }

    @Test
    public void testGetString() {
        String expected = "foo";

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        doReturn(expected).when(mNativeMock).getString(PREF);

        assertEquals(expected, prefServiceBridge.getString(PREF));
    }

    @Test
    public void testSetString() {
        String value = "bar";

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        prefServiceBridge.setString(PREF, value);

        verify(mNativeMock).setString(eq(PREF), eq(value));
    }

    @Test
    public void testIsManaged() {
        boolean expected = true;

        PrefServiceBridge prefServiceBridge = new PrefServiceBridge();
        doReturn(expected).when(mNativeMock).isManagedPreference(PREF);

        assertEquals(expected, prefServiceBridge.isManagedPreference(PREF));
    }
}
