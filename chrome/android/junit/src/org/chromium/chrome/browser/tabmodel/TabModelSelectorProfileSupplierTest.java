// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Unit tests for {@link TabModelSelectorProfileSupplierTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorProfileSupplierTest {
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModel mIncognitoTabModel;
    @Mock
    Profile mProfile;
    @Mock
    Profile mIncognitoProfile;
    @Mock
    Callback<Profile> mProfileCallback1;
    @Mock
    Callback<Profile> mProfileCallback2;

    ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();

    TabModelSelectorProfileSupplier mSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSupplier = new TabModelSelectorProfileSupplier(mTabModelSelectorSupplier);
        doReturn(true).when(mIncognitoTabModel).isIncognito();
        doReturn(true).when(mIncognitoProfile).isOffTheRecord();
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
    }

    @Test
    public void testInitialTabModelHasNoProfile_initializedLater() {
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mSupplier.onTabModelSelected(mTabModel, null);
        Assert.assertNull(mSupplier.get());

        doReturn(mProfile).when(mTabModel).getProfile();
        mSupplier.onTabStateInitialized();
        Assert.assertEquals(mProfile, mSupplier.get());
    }

    @Test
    public void testObserversFired() {
        mSupplier.addObserver(mProfileCallback1);
        mSupplier.addObserver(mProfileCallback2);

        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mProfile).when(mTabModel).getProfile();
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mSupplier.onTabModelSelected(mTabModel, null);

        verify(mProfileCallback1, times(1)).onResult(mProfile);
        verify(mProfileCallback2, times(1)).onResult(mProfile);

        doReturn(mIncognitoProfile).when(mTabModel).getProfile();
        mSupplier.onTabModelSelected(mTabModel, null);

        verify(mProfileCallback1, times(1)).onResult(mIncognitoProfile);
        verify(mProfileCallback2, times(1)).onResult(mIncognitoProfile);
    }

    @Test
    public void tesOTRProfileReturnsForIncognitoTabModel() {
        doReturn(mIncognitoProfile).when(mIncognitoTabModel).getProfile();
        mSupplier.onTabModelSelected(mIncognitoTabModel, mTabModel);

        Assert.assertEquals(mIncognitoProfile, mSupplier.get());
    }

    @Test
    public void tesRegularProfileReturnsForRegularTabModel() {
        doReturn(mProfile).when(mTabModel).getProfile();
        mSupplier.onTabModelSelected(mTabModel, mIncognitoTabModel);

        Assert.assertEquals(mProfile, mSupplier.get());
    }

    @Test
    public void testDestroyPreInitialization() {
        mSupplier.destroy();
        // There's nothing to tear down before the tab model selector is initialized.
        verify(mTabModelSelector, never()).removeObserver(mSupplier);
    }

    @Test
    public void testDestroyPostInitialization() {
        mTabModelSelectorSupplier.set(mTabModelSelector);
        mSupplier.destroy();
        verify(mTabModelSelector).removeObserver(mSupplier);
    }
}
