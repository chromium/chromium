// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinator;
import org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetCoordinatorFactory;
import org.chromium.ui.base.WindowAndroid;

/** Tests for the {@link PasswordMigrationWarningBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordMigrationWarningBridgeTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private PostPasswordMigrationSheetCoordinator mPostPasswordMigrationSheetCoordinator;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(
                mPostPasswordMigrationSheetCoordinator);
    }

    @After
    public void tearDown() {
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(null);
    }

    @Test
    public void showPostPasswordMigrationSheetCreatesTheCoordinator() {
        PasswordMigrationWarningBridge.maybeShowPostMigrationSheet(mWindowAndroid, mProfile);
        verify(mPostPasswordMigrationSheetCoordinator).showSheet();
    }
}
