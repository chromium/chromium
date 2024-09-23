// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.Mockito.doReturn;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;

/** Tests for {@link MerchantTrustSignalsStorageFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class MerchantTrustSignalsStorageFactoryTest {

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private Profile mMockProfile1;

    @Mock private Profile mMockProfile2;

    @Mock private MerchantTrustSignalsEventStorage.Natives mMockStorage;

    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Mock public Profile.Natives mMockProfileNatives;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(MerchantTrustSignalsEventStorageJni.TEST_HOOKS, mMockStorage);
        mMocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);

        doReturn(false).when(mMockProfile1).isOffTheRecord();
        doReturn(false).when(mMockProfile2).isOffTheRecord();
        MerchantTrustSignalsEventStorage.setSkipNativeAssertionsForTesting(true);
    }

    @Test
    public void testGetForLastUsedProfile() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile1);

        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);
        Assert.assertNotNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileNullProfile() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(null);

        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);

        Assert.assertNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileOffTheRecordProfile() {
        doReturn(true).when(mMockProfile1).isOffTheRecord();
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile1);

        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);

        Assert.assertNull(factory.getForLastUsedProfile());
        factory.destroy();
    }

    @Test
    public void testGetForLastUsedProfileSwitch() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile1);

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
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mMockProfile1);

        MerchantTrustSignalsStorageFactory factory =
                new MerchantTrustSignalsStorageFactory(mProfileSupplier);
        factory.getForLastUsedProfile();
        Assert.assertEquals(1, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
        factory.destroy();
        Assert.assertEquals(0, MerchantTrustSignalsStorageFactory.sProfileToStorage.size());
    }
}
