// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/** Unit tests for {@link NtpCustomizationCoordinatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpCustomizationCoordinatorFactoryUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Supplier<Profile> mMockProfileSupplier;

    private Context mContext;
    private NtpCustomizationCoordinatorFactory mFactory;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFactory = new NtpCustomizationCoordinatorFactory();
        NtpCustomizationCoordinatorFactory.setInstanceForTesting(mFactory);
    }

    @Test
    public void testGetInstance_returnsSameInstance() {
        NtpCustomizationCoordinatorFactory instance1 =
                NtpCustomizationCoordinatorFactory.getInstance();
        NtpCustomizationCoordinatorFactory instance2 =
                NtpCustomizationCoordinatorFactory.getInstance();

        assertNotNull("Factory instance should not be null", instance1);
        assertSame(
                "getInstance() should always return the same singleton instance",
                instance1,
                instance2);
    }

    @Test
    public void testCreate_noExistingCoordinator_createsAndStoresNewCoordinator() {
        // Verifies the initial state of mCoordinator.
        assertNull(
                "Factory should have no coordinator initially",
                mFactory.getCoordinatorForTesting());

        // Creates the first coordinator.
        NtpCustomizationCoordinator coordinator =
                mFactory.create(
                        mContext,
                        mMockBottomSheetController,
                        mMockProfileSupplier,
                        NtpCustomizationCoordinator.BottomSheetType.MAIN);

        // Verifies it was created and is now stored.
        assertNotNull("create() should return a non-null coordinator", coordinator);
        assertSame(
                "Factory should store the newly created coordinator",
                coordinator,
                mFactory.getCoordinatorForTesting());
    }

    @Test
    public void testCreate_withExistingCoordinator_dismissesOldAndCreatesNew() {
        NtpCustomizationCoordinator coordinator = mock(NtpCustomizationCoordinator.class);
        mFactory.setCoordinatorForTesting(coordinator);
        mFactory.create(
                mContext,
                mMockBottomSheetController,
                mMockProfileSupplier,
                NtpCustomizationCoordinator.BottomSheetType.MAIN);

        verify(coordinator).dismissBottomSheet();
        assertNotSame(
                "A new coordinator instance should have been created",
                coordinator,
                mFactory.getCoordinatorForTesting());
    }

    @Test
    public void testDestroy_withMatchingCoordinator_clearsReference() {
        // Creates a coordinator and verifies it's stored.
        NtpCustomizationCoordinator coordinator =
                mFactory.create(
                        mContext,
                        mMockBottomSheetController,
                        mMockProfileSupplier,
                        NtpCustomizationCoordinator.BottomSheetType.MAIN);
        assertNotNull(
                "Coordinator should be active in the factory", mFactory.getCoordinatorForTesting());

        mFactory.onNtpCustomizationCoordinatorDestroyed(coordinator);

        // Verifies the factory's reference is now cleared.
        assertNull(
                "Factory should have no active coordinator after destruction",
                mFactory.getCoordinatorForTesting());
    }

    @Test
    public void testDestroy_withStaleCoordinator_doesNotClearReference() {
        // Creates the first coordinator. This one will become "stale".
        NtpCustomizationCoordinator coordinator1 =
                mFactory.create(
                        mContext,
                        mMockBottomSheetController,
                        mMockProfileSupplier,
                        NtpCustomizationCoordinator.BottomSheetType.MAIN);

        // Creates a second coordinator, making the first one stale.
        NtpCustomizationCoordinator coordinator2 =
                mFactory.create(
                        mContext,
                        mMockBottomSheetController,
                        mMockProfileSupplier,
                        NtpCustomizationCoordinator.BottomSheetType.MAIN);

        assertSame(
                "Factory should hold the latest coordinator",
                coordinator2,
                mFactory.getCoordinatorForTesting());

        mFactory.onNtpCustomizationCoordinatorDestroyed(coordinator1);

        // Verifies that the factory should ignore the stale request and keep its current reference.
        assertNotNull(
                "Factory should still have an active coordinator",
                mFactory.getCoordinatorForTesting());
        assertSame(
                "Factory should not have cleared its reference to the active coordinator",
                coordinator2,
                mFactory.getCoordinatorForTesting());
    }
}
