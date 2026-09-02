// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Tests for {@link MerchantTrustSignalsStorageFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MerchantTrustSignalsStorageFactoryTest {
    private static final long FAKE_NATIVE_PTR = 1L;

    @Mock private Profile mMockProfile1;

    @Mock private Profile mMockProfile2;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private MerchantTrustSignalsEventStorage.Natives mMockStorage;

    private SettableNonNullObservableSupplier<Profile> mProfileSupplier;

    @Mock public Profile.Natives mMockProfileNatives;

    @Before
    public void setUp() {
        MerchantTrustSignalsEventStorageJni.setInstanceForTesting(mMockStorage);
        ProfileJni.setInstanceForTesting(mMockProfileNatives);
        // Simulate native init(), which normally calls back into setNativePtr().
        doAnswer(
                        invocation -> {
                            MerchantTrustSignalsEventStorage storage = invocation.getArgument(0);
                            storage.setNativePtrForTesting(FAKE_NATIVE_PTR);
                            return null;
                        })
                .when(mMockStorage)
                .init(any(MerchantTrustSignalsEventStorage.class), any(Profile.class));

        doReturn(false).when(mMockProfile1).isOffTheRecord();
        doReturn(false).when(mMockProfile2).isOffTheRecord();
        mProfileSupplier = ObservableSuppliers.createNonNull(mMockProfile1);
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
        doReturn(true).when(mMockProfile1).isOffTheRecord();
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);

        Assert.assertNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileSwitch() {
        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);

        MerchantTrustSignalsEventStorage db1 = factory.getForLastUsedProfile();
        Assert.assertNotNull(db1);

        mProfileSupplier.set(mMockProfile2);
        MerchantTrustSignalsEventStorage db2 = factory.getForLastUsedProfile();
        Assert.assertNotNull(db2);

        Assert.assertNotEquals(db1, db2);
        factory.destroy();
    }

    @Test
    public void testDestroy() {
        doReturn(false).when(mMockProfile1).isOffTheRecord();

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

        ProfileManager.onProfileDestroyed(mMockProfile1);

        Assert.assertEquals(0, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
        verify(mMockStorage, times(1)).destroy(FAKE_NATIVE_PTR);
        verify(mMockStorage, never()).destroy(0L);
        factory.destroy();
    }

    @Test
    public void testStorageDestroyPreventsFurtherNativeCalls() {
        MerchantTrustSignalsEventStorage storage =
                new MerchantTrustSignalsEventStorage(mMockProfile1);
        storage.destroy();
        storage.deleteAll();
        storage.destroy();

        verify(mMockStorage, times(1)).destroy(FAKE_NATIVE_PTR);
        verify(mMockStorage, never()).destroy(0L);
        verify(mMockStorage, never()).deleteAll(anyLong(), any());
    }
}
