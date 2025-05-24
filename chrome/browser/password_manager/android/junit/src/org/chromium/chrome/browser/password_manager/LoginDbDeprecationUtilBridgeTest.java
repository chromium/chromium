// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertFalse;
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
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

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
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;

    @Before
    public void setUp() {
        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
    }

    @Test
    public void testHasCsvFileExists() throws IOException {
        File tempFile = File.createTempFile("passwords", "csv");
        tempFile.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(mProfile))
                .thenReturn(tempFile.getAbsolutePath());
        when(mPrefService.getBoolean(Pref.UPM_AUTO_EXPORT_CSV_NEEDS_DELETION)).thenReturn(false);

        assertTrue(LoginDbDeprecationUtilBridge.hasPasswordsInCsv(mProfile));
    }

    @Test
    public void testHasCsvFileExistsButShouldBeDeleted() throws IOException {
        File fakeFile = File.createTempFile("passwords", "csv", null);
        fakeFile.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(mProfile))
                .thenReturn(fakeFile.getAbsolutePath());
        when(mPrefService.getBoolean(Pref.UPM_AUTO_EXPORT_CSV_NEEDS_DELETION)).thenReturn(true);

        assertFalse(LoginDbDeprecationUtilBridge.hasPasswordsInCsv(mProfile));
    }
}
