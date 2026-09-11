// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.TestProfile;

/** Tests for {@link MerchantTrustSignalsStorageFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MerchantTrustSignalsStorageFactoryTest {
    private static final long FAKE_NATIVE_PTR = 1L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final TestProfile mProfile1 = TestProfile.createRegular();
    private final TestProfile mProfile2 = TestProfile.createRegular();

    @Mock private MerchantTrustSignalsEventStorage.Natives mMockStorage;

    private SettableNonNullObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        MerchantTrustSignalsEventStorageJni.setInstanceForTesting(mMockStorage);
        // Simulate native init(), which normally calls back into setNativePtr().
        doAnswer(
                        invocation -> {
                            MerchantTrustSignalsEventStorage storage = invocation.getArgument(0);
                            storage.setNativePtrForTesting(FAKE_NATIVE_PTR);
                            return null;
                        })
                .when(mMockStorage)
                .init(any(MerchantTrustSignalsEventStorage.class), any(Profile.class));

        mProfileSupplier = ObservableSuppliers.createNonNull(mProfile1);
    }

    @Test
    public void testGetForLastUsedProfile() {
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);
        Assert.assertNotNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileOffTheRecordProfile() {
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(
                        ObservableSuppliers.createNonNull(TestProfile.createIncognito()));

        Assert.assertNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileSwitch() {
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);

        MerchantTrustSignalsEventStorage db1 = factory.getForLastUsedProfile();
        Assert.assertNotNull(db1);

        mProfileSupplier.set(mProfile2);
        MerchantTrustSignalsEventStorage db2 = factory.getForLastUsedProfile();
        Assert.assertNotNull(db2);

        Assert.assertNotEquals(db1, db2);
        factory.destroy();
    }

    @Test
    public void testDestroy() {

        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);
        factory.getForLastUsedProfile();
        Assert.assertEquals(1, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
        factory.destroy();
        Assert.assertEquals(0, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
        verify(mMockStorage, times(1)).destroy(FAKE_NATIVE_PTR);
        verify(mMockStorage, never()).destroy(0L);
    }

    @Test
    public void testDestroyOnProfileDestroyed() {
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);
        factory.getForLastUsedProfile();
        Assert.assertEquals(1, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());

        ProfileManager.onProfileDestroyed(mProfile1);

        Assert.assertEquals(0, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
        verify(mMockStorage, times(1)).destroy(FAKE_NATIVE_PTR);
        verify(mMockStorage, never()).destroy(0L);
        factory.destroy();
    }

    @Test
    public void testStorageDestroyPreventsFurtherNativeCalls() {
        MerchantTrustSignalsEventStorage storage = new MerchantTrustSignalsEventStorage(mProfile1);
        storage.destroy();
        storage.deleteAll();
        storage.destroy();

        verify(mMockStorage, times(1)).destroy(FAKE_NATIVE_PTR);
        verify(mMockStorage, never()).destroy(0L);
        verify(mMockStorage, never()).deleteAll(anyLong(), any());
    }
}
