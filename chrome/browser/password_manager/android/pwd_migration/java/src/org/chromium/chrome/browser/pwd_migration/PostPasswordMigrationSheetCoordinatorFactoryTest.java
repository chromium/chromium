// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for the methods of {@link PostPasswordMigrationSheetCoordinatorFactoryTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PostPasswordMigrationSheetCoordinatorFactoryTest {

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;

    private PostPasswordMigrationSheetCoordinator mPostPasswordMigrationSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application.getApplicationContext();
        WeakReference<Context> weakContext = new WeakReference<Context>(context);
        when(mWindowAndroid.getContext()).thenReturn(weakContext);
        mPostPasswordMigrationSheetCoordinator =
                new PostPasswordMigrationSheetCoordinator(
                        context, mBottomSheetController, mProfile);
    }

    @After
    public void tearDown() {
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(null);
    }

    @Test
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testmaybeGetOrCreateReturnsNullWhenBottomSheetControllerIsNull() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        assertNull(
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(
                                mWindowAndroid, mProfile));
    }

    @Test
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testmaybeGetOrCreateReturnsNullWhenContextIsNull() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(null));
        assertNull(
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(
                                mWindowAndroid, mProfile));
    }

    @Test
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testSetCoordinatorInstanceForTestingUsesTheTestingFactory() {
        PostPasswordMigrationSheetCoordinatorFactory.setCoordinatorInstanceForTesting(
                mPostPasswordMigrationSheetCoordinator);
        assertEquals(
                PostPasswordMigrationSheetCoordinatorFactory
                        .maybeGetOrCreatePostPasswordMigrationSheetCoordinator(
                                mWindowAndroid, mProfile),
                mPostPasswordMigrationSheetCoordinator);
    }
}
