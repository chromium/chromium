// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Tests for the methods of {@link PostPasswordMigrationSheetCoordinatorFactoryTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PostPasswordMigrationSheetCoordinatorFactoryTest {

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;

    private PostPasswordMigrationSheetCoordinator mPostPasswordMigrationSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPostPasswordMigrationSheetCoordinator =
                new PostPasswordMigrationSheetCoordinator(mBottomSheetController);
    }

    @After
    public void tearDown() {
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(null);
    }

    @Test
    public void testmaybeGetOrCreateReturnsNullWhenBottomSheetControllerIsNull() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        assertNull(
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(mWindowAndroid));
    }

    @Test
    public void testSetCoordinatorInstanceForTestingUsesTheTestingFactory() {
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(
                mPostPasswordMigrationSheetCoordinator);
        assertEquals(
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(mWindowAndroid),
                mPostPasswordMigrationSheetCoordinator);
    }
}
