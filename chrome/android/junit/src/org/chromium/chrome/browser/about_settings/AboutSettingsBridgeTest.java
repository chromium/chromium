// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// generate_java_test.py

package org.chromium.chrome.browser.about_settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for {@link AboutSettingsBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AboutSettingsBridgeTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Mock private AboutSettingsBridge.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(AboutSettingsBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testGetApplicationVersion() {
        String expected = "Clankium 10.0.1111.0";
        doReturn(expected).when(mNativeMock).getApplicationVersion();
        assertEquals(expected, AboutSettingsBridge.getApplicationVersion());
    }

    @Test
    public void testGetOSVersion() {
        String expected = "Android 9; Android SDK built for x86 Build/PSR1.180720.093";
        doReturn(expected).when(mNativeMock).getOSVersion();
        assertEquals(expected, AboutSettingsBridge.getOSVersion());
    }
}
