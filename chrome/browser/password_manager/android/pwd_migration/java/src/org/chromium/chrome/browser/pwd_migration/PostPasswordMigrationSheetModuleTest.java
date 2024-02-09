// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.pwd_migration.PostPasswordMigrationSheetProperties.VISIBLE;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests for the PostPasswordMigrationSheet. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PostPasswordMigrationSheetModuleTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private BottomSheetController mBottomSheetController;

    private PostPasswordMigrationSheetCoordinator mPostPasswordMigrationSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPostPasswordMigrationSheetCoordinator =
                new PostPasswordMigrationSheetCoordinator(mBottomSheetController);
    }

    @Test
    public void showPostPasswordMigrationSheetCreatesTheCoordinator() {
        mPostPasswordMigrationSheetCoordinator.showSheet();
        assertTrue(mPostPasswordMigrationSheetCoordinator.getModelForTesting().get(VISIBLE));
        // TODO(b/323821929): Verify that the BottomSheetController.requestShowContent was called.
    }
}
