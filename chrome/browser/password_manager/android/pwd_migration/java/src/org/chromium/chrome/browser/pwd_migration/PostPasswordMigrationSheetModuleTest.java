// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING;
import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests for the PostPasswordMigrationSheet. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PostPasswordMigrationSheetModuleTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;

    private PostPasswordMigrationSheetCoordinator mPostPasswordMigrationSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application.getApplicationContext();
        mPostPasswordMigrationSheetCoordinator =
                new PostPasswordMigrationSheetCoordinator(
                        context, mBottomSheetController, mProfile);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
    }

    @Test
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void showPostPasswordMigrationSheetCreatesTheCoordinator() {
        mPostPasswordMigrationSheetCoordinator.showSheet();
        assertTrue(mPostPasswordMigrationSheetCoordinator.getModelForTesting().get(VISIBLE));
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }
}
