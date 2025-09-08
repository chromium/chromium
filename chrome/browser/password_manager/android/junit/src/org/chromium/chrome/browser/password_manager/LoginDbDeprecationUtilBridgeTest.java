// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.File;
import java.io.IOException;

/** Unit tests for the {@link LoginDbDeprecationUtilBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class LoginDbDeprecationUtilBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LoginDbDeprecationUtilBridge.Natives mLoginDbDeprecationUtilBridgeJniMock;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridgeJniMock);
    }

    @Test
    public void testHasCsvFileExists() throws IOException {
        File tempFile = File.createTempFile("passwords", "csv");
        tempFile.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(mProfile))
                .thenReturn(tempFile.getAbsolutePath());

        assertTrue(LoginDbDeprecationUtilBridge.hasPasswordsInCsv(mProfile));
    }
}
